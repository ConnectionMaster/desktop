// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ServerTimingHeader.h"

namespace blink {

void ServerTimingHeader::SetParameter(StringView name, String value) {
  if (EqualIgnoringASCIICase(name, "dur")) {
    if (!duration_set_) {
      duration_ = value.ToDouble();
      duration_set_ = true;
    }
  } else if (EqualIgnoringASCIICase(name, "desc")) {
    if (!description_set_) {
      description_ = value;
      description_set_ = true;
    }
  }
}

}  // namespace blink
