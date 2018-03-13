// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/AuthenticatorAssertionResponse.h"

namespace blink {

AuthenticatorAssertionResponse* AuthenticatorAssertionResponse::Create(
    DOMArrayBuffer* client_data_json,
    DOMArrayBuffer* authenticator_data,
    DOMArrayBuffer* signature,
    DOMArrayBuffer* user_handle) {
  return new AuthenticatorAssertionResponse(
      client_data_json, authenticator_data, signature, user_handle);
}

AuthenticatorAssertionResponse::AuthenticatorAssertionResponse(
    DOMArrayBuffer* client_data_json,
    DOMArrayBuffer* authenticator_data,
    DOMArrayBuffer* signature,
    DOMArrayBuffer* user_handle)
    : AuthenticatorResponse(client_data_json),
      authenticator_data_(authenticator_data),
      signature_(signature),
      user_handle_(user_handle) {}

AuthenticatorAssertionResponse::~AuthenticatorAssertionResponse() {}

void AuthenticatorAssertionResponse::Trace(blink::Visitor* visitor) {
  visitor->Trace(authenticator_data_);
  visitor->Trace(signature_);
  visitor->Trace(user_handle_);
  AuthenticatorResponse::Trace(visitor);
}

}  // namespace blink
