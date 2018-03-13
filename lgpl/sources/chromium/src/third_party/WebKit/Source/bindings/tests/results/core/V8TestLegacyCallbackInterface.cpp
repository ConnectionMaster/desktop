// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/legacy_callback_interface.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "V8TestLegacyCallbackInterface.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8DOMConfiguration.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is
// trivial and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestLegacyCallbackInterface::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestLegacyCallbackInterface::DomTemplate,
    nullptr,
    nullptr,
    nullptr,
    "TestLegacyCallbackInterface",
    nullptr,
    WrapperTypeInfo::kWrapperTypeNoPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

void V8TestLegacyCallbackInterface::TypeErrorConstructorCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8ThrowException::ThrowTypeError(info.GetIsolate(),
      "Illegal constructor: TestLegacyCallbackInterface");
}

static void InstallV8TestLegacyCallbackInterfaceTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Legacy callback interface must not have a prototype object.
  interfaceTemplate->RemovePrototype();

  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate,
      V8TestLegacyCallbackInterface::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(),
      kV8DefaultWrapperInternalFieldCount);
  interfaceTemplate->SetCallHandler(V8TestLegacyCallbackInterface::TypeErrorConstructorCallback);
  interfaceTemplate->SetLength(0);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register DOM constants.
  static constexpr V8DOMConfiguration::ConstantConfiguration V8TestLegacyCallbackInterfaceConstants[] = {
      {"CONST_VALUE_USHORT_42", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(42)},
  };
  V8DOMConfiguration::InstallConstants(
      isolate, interfaceTemplate, prototypeTemplate,
      V8TestLegacyCallbackInterfaceConstants, WTF_ARRAY_LENGTH(V8TestLegacyCallbackInterfaceConstants));
  static_assert(42 == TestLegacyCallbackInterface::kConstValueUshort42, "the value of TestLegacyCallbackInterface_kConstValueUshort42 does not match with implementation");
}

v8::Local<v8::FunctionTemplate> V8TestLegacyCallbackInterface::DomTemplate(v8::Isolate* isolate,
    const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate,
      world,
      const_cast<WrapperTypeInfo*>(&wrapperTypeInfo),
      InstallV8TestLegacyCallbackInterfaceTemplate);
}

}  // namespace blink
