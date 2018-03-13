// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/WebkitBoxFlex.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* WebkitBoxFlex::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeAll);
}

}  // namespace CSSLonghand
}  // namespace blink
