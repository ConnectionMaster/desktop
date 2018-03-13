// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/TypesettingFeatures.h"

#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

namespace {

const char* kFeatureNames[kMaxTypesettingFeatureIndex + 1] = {
    "Kerning", "Ligatures", "Caps"};

}  // namespace

String ToString(TypesettingFeatures features) {
  StringBuilder builder;
  int featureCount = 0;
  for (int i = 0; i <= kMaxTypesettingFeatureIndex; i++) {
    if (features & (1 << i)) {
      if (featureCount++ > 0)
        builder.Append(",");
      builder.Append(kFeatureNames[i]);
    }
  }
  return builder.ToString();
}

}  // namespace blink
