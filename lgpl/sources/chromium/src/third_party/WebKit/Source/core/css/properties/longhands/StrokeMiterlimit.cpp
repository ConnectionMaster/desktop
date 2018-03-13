// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/StrokeMiterlimit.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* StrokeMiterlimit::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
}

}  // namespace CSSLonghand
}  // namespace blink
