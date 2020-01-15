// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_number_optional_number_interpolation_type.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/svg/svg_number_optional_number.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

InterpolationValue
SVGNumberOptionalNumberInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  auto result = std::make_unique<InterpolableList>(2);
  result->Set(0, std::make_unique<InterpolableNumber>(0));
  result->Set(1, std::make_unique<InterpolableNumber>(0));
  return InterpolationValue(std::move(result));
}

InterpolationValue
SVGNumberOptionalNumberInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedNumberOptionalNumber)
    return nullptr;

  const SVGNumberOptionalNumber& number_optional_number =
      ToSVGNumberOptionalNumber(svg_value);
  auto result = std::make_unique<InterpolableList>(2);
  result->Set(0, std::make_unique<InterpolableNumber>(
                     number_optional_number.FirstNumber()->Value()));
  result->Set(1, std::make_unique<InterpolableNumber>(
                     number_optional_number.SecondNumber()->Value()));
  return InterpolationValue(std::move(result));
}

SVGPropertyBase* SVGNumberOptionalNumberInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  return MakeGarbageCollected<SVGNumberOptionalNumber>(
      MakeGarbageCollected<SVGNumber>(
          ToInterpolableNumber(list.Get(0))->Value()),
      MakeGarbageCollected<SVGNumber>(
          ToInterpolableNumber(list.Get(1))->Value()));
}

}  // namespace blink
