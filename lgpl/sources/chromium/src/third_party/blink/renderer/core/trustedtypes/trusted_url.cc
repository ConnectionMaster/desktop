// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_url.h"

namespace blink {

TrustedURL::TrustedURL(const String& url) : url_(url) {}

String TrustedURL::toString() const {
  return url_;
}

}  // namespace blink
