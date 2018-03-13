// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/TabSize.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* TabSize::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSPrimitiveValue* parsed_value =
      CSSPropertyParserHelpers::ConsumeInteger(range, 0);
  if (parsed_value)
    return parsed_value;
  return CSSPropertyParserHelpers::ConsumeLength(range, context.Mode(),
                                                 kValueRangeNonNegative);
}

}  // namespace CSSLonghand
}  // namespace blink
