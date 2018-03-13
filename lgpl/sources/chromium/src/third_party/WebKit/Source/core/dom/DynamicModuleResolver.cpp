// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DynamicModuleResolver.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ReferrerScriptInfo.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/Modulator.h"
#include "core/dom/ModuleScript.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "platform/bindings/V8ThrowException.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class DynamicImportTreeClient final : public ModuleTreeClient {
 public:
  static DynamicImportTreeClient* Create(
      const KURL& url,
      Modulator* modulator,
      ScriptPromiseResolver* promise_resolver) {
    return new DynamicImportTreeClient(url, modulator, promise_resolver);
  }

  void Trace(blink::Visitor*);

 private:
  DynamicImportTreeClient(const KURL& url,
                          Modulator* modulator,
                          ScriptPromiseResolver* promise_resolver)
      : url_(url), modulator_(modulator), promise_resolver_(promise_resolver) {}

  // Implements ModuleTreeClient:
  void NotifyModuleTreeLoadFinished(ModuleScript*) final;

  const KURL url_;
  const Member<Modulator> modulator_;
  const Member<ScriptPromiseResolver> promise_resolver_;
};

void DynamicImportTreeClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  // Implements steps 2.[5-8] of
  // https://html.spec.whatwg.org/multipage/webappapis.html#hostimportmoduledynamically(referencingscriptormodule,-specifier,-promisecapability)

  // [nospec] Abort the steps if the browsing context is discarded.
  if (!modulator_->HasValidContext()) {
    // The promise_resolver_ should have ::Detach()-ed at this point,
    // so ::Reject() is not necessary.
    return;
  }

  ScriptState* script_state = modulator_->GetScriptState();
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();

  // Step 2.5. "If result is null, then:" [spec text]
  if (!module_script) {
    // Step 2.5.1. "Let completion be Completion { [[Type]]: throw, [[Value]]: a
    // new TypeError, [[Target]]: empty }." [spec text]
    v8::Local<v8::Value> error = V8ThrowException::CreateTypeError(
        isolate,
        "Failed to fetch dynamically imported module: " + url_.GetString());

    // Step 2.5.2. "Perform FinishDynamicImport(referencingScriptOrModule,
    // specifier, promiseCapability, completion)." [spec text]
    promise_resolver_->Reject(error);

    // Step 2.5.3. "Abort these steps."
    return;
  }

  // Step 2.6. "Run the module script module script, with the rethrow errors
  // boolean set to true." [spec text]
  ScriptValue error = modulator_->ExecuteModule(
      module_script, Modulator::CaptureEvalErrorFlag::kCapture);

  // Step 2.7. "If running the module script throws an exception, ..." [spec
  // text]
  if (!error.IsEmpty()) {
    // "... then perform FinishDynamicImport(referencingScriptOrModule,
    // specifier, promiseCapability, the thrown exception completion)."
    // [spec text]
    // Note: "the thrown exception completion" is |error|.
    // https://tc39.github.io/proposal-dynamic-import/#sec-finishdynamicimport
    // Step 1. "If completion is an abrupt completion, then perform !
    // Call(promiseCapability.[[Reject]], undefined, << completion.[[Value]]
    // >>)." [spec text]
    promise_resolver_->Reject(error);
    return;
  }

  // Step 2.8. "Otherwise, perform
  // FinishDynamicImport(referencingScriptOrModule, specifier,
  // promiseCapability, NormalCompletion(undefined))." [spec text]
  // https://tc39.github.io/proposal-dynamic-import/#sec-finishdynamicimport
  // Step 2.a. "Assert: completion is a normal completion and
  // completion.[[Value]] is undefined." [spec text]
  DCHECK(error.IsEmpty());

  // Step 2.b. "Let moduleRecord be
  // !HostResolveImportedModule(referencingScriptOrModule, specifierString)."
  // [spec text]
  // Note: We skip invocation of ScriptModuleResolver here. The
  // result of HostResolveImportedModule is guaranteed to be |module_script|.
  ScriptModule record = module_script->Record();
  DCHECK(!record.IsNull());

  // Step 2.c. "Assert: ModuleEvaluation has already been invoked on
  // moduleRecord and successfully completed." [spec text]
  DCHECK_EQ(ScriptModuleState::kEvaluated, modulator_->GetRecordStatus(record));

  // Step 2.d. "Let namespace be GetModuleNamespace(moduleRecord)." [spec text]
  v8::Local<v8::Value> module_namespace = record.V8Namespace(isolate);

  // Step 2.e. "If namespace is an abrupt completion, perform
  // !Call(promiseCapability.[[Reject]], undefined, << namespace.[[Value]] >>)."
  // [spec text]
  // Note: Blink's implementation never allows |module_namespace| to be
  // an abrupt completion.

  // Step 2.f "Otherwise, perform ! Call(promiseCapability.[[Resolve]],
  // undefined, << namespace.[[Value]] >>)." [spec text]
  promise_resolver_->Resolve(module_namespace);
}

void DynamicImportTreeClient::Trace(blink::Visitor* visitor) {
  visitor->Trace(modulator_);
  visitor->Trace(promise_resolver_);
  ModuleTreeClient::Trace(visitor);
}

}  // namespace

void DynamicModuleResolver::Trace(blink::Visitor* visitor) {
  visitor->Trace(modulator_);
}

void DynamicModuleResolver::ResolveDynamically(
    const String& specifier,
    const KURL& referrer_url,
    const ReferrerScriptInfo& referrer_info,
    ScriptPromiseResolver* promise_resolver) {
  DCHECK(modulator_->GetScriptState()->GetIsolate()->InContext())
      << "ResolveDynamically should be called from V8 callback, within a valid "
         "context.";

  // https://html.spec.whatwg.org/multipage/webappapis.html#hostimportmoduledynamically(referencingscriptormodule,-specifier,-promisecapability)
  // Step 1. "Let referencing script be
  // referencingScriptOrModule.[[HostDefined]]." [spec text]

  // Step 2. "Run the following steps in parallel:"

  // Step 2.1. "Let url be the result of resolving a module specifier
  // given referencing script and specifier." [spec text]
  KURL context_url =
      referrer_url.IsValid()
          ? referrer_url
          : ExecutionContext::From(modulator_->GetScriptState())->Url();
  DCHECK(context_url.IsValid());

  KURL url = Modulator::ResolveModuleSpecifier(specifier, context_url);
  if (!url.IsValid()) {
    // Step 2.2.1. "If the result is failure, then:" [spec text]
    // Step 2.2.2.1. "Let completion be Completion { [[Type]]: throw, [[Value]]:
    // a new TypeError, [[Target]]: empty }." [spec text]
    v8::Isolate* isolate = modulator_->GetScriptState()->GetIsolate();
    v8::Local<v8::Value> error = V8ThrowException::CreateTypeError(
        isolate, "Failed to resolve module specifier '" + specifier + "'");

    // Step 2.2.2.2. "Perform FinishDynamicImport(referencingScriptOrModule,
    // specifier, promiseCapability, completion)" [spec text]
    // https://tc39.github.io/proposal-dynamic-import/#sec-finishdynamicimport
    // Step 1. "If completion is an abrupt completion, then perform
    // !Call(promiseCapability.[[Reject]], undefined, <<completion.[[Value]]>>).
    // " [spec text]
    promise_resolver->Reject(error);

    // Step 2.2.2.3. "Abort these steps." [spec text]
    return;
  }

  // Step 2.3. "Let options be the descendant script fetch options for
  // referencing script's fetch options." [spec text]
  // https://html.spec.whatwg.org/multipage/webappapis.html#descendant-script-fetch-options
  // "For any given script fetch options options, the descendant script fetch
  // options are a new script fetch options whose items all have the same
  // values, except for the integrity metadata, which is instead the empty
  // string." [spec text]
  ScriptFetchOptions options(referrer_info.Nonce(), IntegrityMetadataSet(),
                             String(), referrer_info.ParserState(),
                             referrer_info.CredentialsMode());
  ModuleScriptFetchRequest request(url, modulator_->GetReferrerPolicy(),
                                   options);

  // Step 2.4. "Fetch a module script graph given url, settings object,
  // "script", and options. Wait until the algorithm asynchronously completes
  // with result."
  auto tree_client =
      DynamicImportTreeClient::Create(url, modulator_.Get(), promise_resolver);
  modulator_->FetchTree(request, tree_client);

  // Steps 2.[5-8] are implemented at
  // DynamicImportTreeClient::NotifyModuleLoadFinished.

  // Step 3. "Return undefined." [spec text]
}

}  // namespace blink
