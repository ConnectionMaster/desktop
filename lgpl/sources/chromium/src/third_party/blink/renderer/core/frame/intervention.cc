// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/intervention.h"

#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/intervention_report_body.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"

namespace blink {

// static
void Intervention::GenerateReport(const LocalFrame* frame,
                                  const String& id,
                                  const String& message) {
  if (!frame)
    return;

  // Send the message to the console.
  Document* document = frame->GetDocument();
  document->AddConsoleMessage(
      ConsoleMessage::Create(mojom::ConsoleMessageSource::kIntervention,
                             mojom::ConsoleMessageLevel::kError, message));

  if (!frame->Client())
    return;

  // Construct the intervention report.
  InterventionReportBody* body =
      MakeGarbageCollected<InterventionReportBody>(id, message);
  Report* report = MakeGarbageCollected<Report>(
      ReportType::kIntervention, document->Url().GetString(), body);

  // Send the intervention report to the Reporting API and any
  // ReportingObservers.
  ReportingContext::From(document)->QueueReport(report);
}

}  // namespace blink
