// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/ShapeOutside.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace CSSLonghand {

using namespace CSSPropertyParserHelpers;

const CSSValue* ShapeOutside::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (CSSValue* image_value = ConsumeImageOrNone(range, &context))
    return image_value;
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (CSSValue* box_value = ConsumeShapeBox(range))
    list->Append(*box_value);
  if (CSSValue* shape_value =
          CSSParsingUtils::ConsumeBasicShape(range, context)) {
    list->Append(*shape_value);
    if (list->length() < 2) {
      if (CSSValue* box_value = ConsumeShapeBox(range))
        list->Append(*box_value);
    }
  }
  if (!list->length())
    return nullptr;
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
