// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/callback_function.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#include "third_party/blink/renderer/bindings/tests/results/modules/v8_void_callback_function_modules.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"

namespace blink {

const char* V8VoidCallbackFunctionModules::NameInHeapSnapshot() const {
  return "V8VoidCallbackFunctionModules";
}

v8::Maybe<void> V8VoidCallbackFunctionModules::Invoke(bindings::V8ValueOrScriptWrappableAdapter callback_this_value) {
  ScriptState* callback_relevant_script_state =
      CallbackRelevantScriptStateOrThrowException(
          "VoidCallbackFunctionModules",
          "invoke");
  if (!callback_relevant_script_state) {
    return v8::Nothing<void>();
  }

  if (!IsCallbackFunctionRunnable(callback_relevant_script_state,
                                  IncumbentScriptState())) {
    // Wrapper-tracing for the callback function makes the function object and
    // its creation context alive. Thus it's safe to use the creation context
    // of the callback function here.
    v8::HandleScope handle_scope(GetIsolate());
    v8::Local<v8::Object> callback_object = CallbackObject();
    CHECK(!callback_object.IsEmpty());
    v8::Context::Scope context_scope(callback_object->CreationContext());
    V8ThrowException::ThrowError(
        GetIsolate(),
        ExceptionMessages::FailedToExecute(
            "invoke",
            "VoidCallbackFunctionModules",
            "The provided callback is no longer runnable."));
    return v8::Nothing<void>();
  }

  // step: Prepare to run script with relevant settings.
  ScriptState::Scope callback_relevant_context_scope(
      callback_relevant_script_state);
  // step: Prepare to run a callback with stored settings.
  v8::Context::BackupIncumbentScope backup_incumbent_scope(
      IncumbentScriptState()->GetContext());

  if (UNLIKELY(ScriptForbiddenScope::IsScriptForbidden())) {
    ScriptForbiddenScope::ThrowScriptForbiddenException(GetIsolate());
    return v8::Nothing<void>();
  }

  v8::Local<v8::Function> function;
  // callback function's invoke:
  // step 4. If ! IsCallable(F) is false:
  //
  // No [TreatNonObjectAsNull] presents.  Must be always callable.
  DCHECK(CallbackObject()->IsFunction());
  function = CallbackFunction();

  v8::Local<v8::Value> this_arg;
  if (callback_this_value.IsEmpty()) {
    // step 2. If thisArg was not given, let thisArg be undefined.
    this_arg = v8::Undefined(GetIsolate());
  } else {
    this_arg = callback_this_value.V8Value(callback_relevant_script_state);
  }

  // step: Let esArgs be the result of converting args to an ECMAScript
  //   arguments list. If this throws an exception, set completion to the
  //   completion value representing the thrown exception and jump to the step
  //   labeled return.
  const int argc = 0;
  v8::Local<v8::Value> *argv = nullptr;

  v8::Local<v8::Value> call_result;
  // step: Let callResult be Call(X, thisArg, esArgs).
  if (!V8ScriptRunner::CallFunction(
          function,
          ExecutionContext::From(callback_relevant_script_state),
          this_arg,
          argc,
          argv,
          GetIsolate()).ToLocal(&call_result)) {
    // step: If callResult is an abrupt completion, set completion to callResult
    //   and jump to the step labeled return.
    return v8::Nothing<void>();
  }

  // step: Set completion to the result of converting callResult.[[Value]] to
  //   an IDL value of the same type as the operation's return type.
  return v8::JustVoid();
}

void V8VoidCallbackFunctionModules::InvokeAndReportException(bindings::V8ValueOrScriptWrappableAdapter callback_this_value) {
  v8::TryCatch try_catch(GetIsolate());
  try_catch.SetVerbose(true);

  v8::Maybe<void> maybe_result =
      Invoke(callback_this_value);
  // An exception if any is killed with the v8::TryCatch above.
  ALLOW_UNUSED_LOCAL(maybe_result);
}

}  // namespace blink
