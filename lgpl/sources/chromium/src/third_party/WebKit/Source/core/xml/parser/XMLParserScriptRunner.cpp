// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/xml/parser/XMLParserScriptRunner.h"

#include "core/dom/ClassicPendingScript.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptLoader.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/xml/parser/XMLParserScriptRunnerHost.h"

namespace blink {

// Spec links:
// [Parsing] https://html.spec.whatwg.org/#parsing-xhtml-documents
// [Prepare] https://html.spec.whatwg.org/#prepare-a-script

XMLParserScriptRunner::XMLParserScriptRunner(XMLParserScriptRunnerHost* host)
    : host_(host) {}

XMLParserScriptRunner::~XMLParserScriptRunner() {
  DCHECK(!parser_blocking_script_);
}

void XMLParserScriptRunner::Trace(Visitor* visitor) {
  visitor->Trace(parser_blocking_script_);
  visitor->Trace(host_);
  PendingScriptClient::Trace(visitor);
}

void XMLParserScriptRunner::Detach() {
  if (parser_blocking_script_) {
    parser_blocking_script_->StopWatchingForLoad();
    parser_blocking_script_ = nullptr;
  }
}

void XMLParserScriptRunner::PendingScriptFinished(
    PendingScript* unused_pending_script) {
  DCHECK_EQ(unused_pending_script, parser_blocking_script_);
  PendingScript* pending_script = parser_blocking_script_;
  parser_blocking_script_ = nullptr;

  pending_script->StopWatchingForLoad();

  ScriptLoader* script_loader = pending_script->GetElement()->Loader();
  DCHECK(script_loader);
  CHECK_EQ(script_loader->GetScriptType(), ScriptType::kClassic);

  // [Parsing] 4. Execute the pending parsing-blocking script. [spec text]
  script_loader->ExecuteScriptBlock(pending_script, NullURL());

  // [Parsing] 5. There is no longer a pending parsing-blocking script. [spec
  // text]
  DCHECK(!parser_blocking_script_);

  // [Parsing] 3. Unblock this instance of the XML parser, such that tasks that
  // invoke it can again be run. [spec text]
  host_->NotifyScriptExecuted();
}

void XMLParserScriptRunner::ProcessScriptElement(
    Document& document,
    Element* element,
    TextPosition script_start_position) {
  DCHECK(element);
  DCHECK(!parser_blocking_script_);

  ScriptElementBase* script_element_base =
      ScriptElementBase::FromElementIfPossible(element);
  CHECK(script_element_base);

  ScriptLoader* script_loader = script_element_base->Loader();
  DCHECK(script_loader);

  // [Parsing] When the element's end tag is subsequently parsed, the user agent
  // must perform a microtask checkpoint, and then prepare the script element.
  // [spec text]
  bool success = script_loader->PrepareScript(
      script_start_position, ScriptLoader::kAllowLegacyTypeInTypeAttribute);

  if (script_loader->GetScriptType() != ScriptType::kClassic) {
    // XMLDocumentParser does not support a module script, and thus ignores it.
    success = false;
    document.AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel,
                               "Module scripts in XML documents are currently "
                               "not supported. See crbug.com/717643"));
  }

  if (!success)
    return;

  // [Parsing] If this causes there to be a pending parsing-blocking script,
  // then the user agent must run the following steps: [spec text]
  if (script_loader->ReadyToBeParserExecuted()) {
    // [Prepare] 5th Clause, Step 24.
    //
    // [Parsing] 4. Execute the pending parsing-blocking script. [spec text]

    // TODO(hiroshige): XMLParserScriptRunner doesn't check style sheet that
    // is blocking scripts and thus the script is executed immediately here,
    // and thus Steps 1-3 are skipped.
    script_loader->ExecuteScriptBlock(script_loader->TakePendingScript(),
                                      document.Url());
  } else if (script_loader->WillBeParserExecuted()) {
    // [Prepare] 2nd Clause, Step 24:
    // The element is the pending parsing-blocking script of the Document
    // of the parser that created the element. (There can only be one such
    // script per Document at a time.) [spec text]
    parser_blocking_script_ = script_loader->TakePendingScript();
    parser_blocking_script_->MarkParserBlockingLoadStartTime();

    // [Parsing] 1. Block this instance of the XML parser, such that the event
    // loop will not run tasks that invoke it. [spec text]
    //
    // This is done in XMLDocumentParser::EndElementNs().

    // [Parsing] 2. Spin the event loop until the parser's Document has no style
    // sheet that is blocking scripts and the pending parsing-blocking script's
    // "ready to be parser-executed" flag is set. [spec text]
    //
    // TODO(hiroshige): XMLParserScriptRunner doesn't check style sheet that
    // is blocking scripts.
    parser_blocking_script_->WatchForLoad(this);
  }
}

}  // namespace blink
