// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/PermissionUtils.h"

#include <utility>

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

// There are two PermissionDescriptor, one in Mojo bindings and one
// in v8 bindings so we'll rename one here.
using MojoPermissionDescriptor = mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;

void ConnectToPermissionService(
    ExecutionContext* execution_context,
    mojom::blink::PermissionServiceRequest request) {
  if (auto* interface_provider = execution_context->GetInterfaceProvider())
    interface_provider->GetInterface(std::move(request));
}

PermissionDescriptorPtr CreatePermissionDescriptor(PermissionName name) {
  auto descriptor = MojoPermissionDescriptor::New();
  descriptor->name = name;
  return descriptor;
}

PermissionDescriptorPtr CreateMidiPermissionDescriptor(bool sysex) {
  auto descriptor =
      CreatePermissionDescriptor(mojom::blink::PermissionName::MIDI);
  auto midi_extension = mojom::blink::MidiPermissionDescriptor::New();
  midi_extension->sysex = sysex;
  descriptor->extension = mojom::blink::PermissionDescriptorExtension::New();
  descriptor->extension->set_midi(std::move(midi_extension));
  return descriptor;
}

PermissionDescriptorPtr CreateClipboardPermissionDescriptor(
    PermissionName name,
    bool allow_without_gesture) {
  auto descriptor = CreatePermissionDescriptor(name);
  auto clipboard_extension =
      mojom::blink::ClipboardPermissionDescriptor::New(allow_without_gesture);
  descriptor->extension = mojom::blink::PermissionDescriptorExtension::New();
  descriptor->extension->set_clipboard(std::move(clipboard_extension));
  return descriptor;
}

}  // namespace blink
