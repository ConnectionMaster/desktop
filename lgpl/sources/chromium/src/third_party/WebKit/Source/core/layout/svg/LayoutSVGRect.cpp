/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/svg/LayoutSVGRect.h"

#include "core/svg/SVGRectElement.h"
#include "platform/wtf/MathExtras.h"

namespace blink {

LayoutSVGRect::LayoutSVGRect(SVGRectElement* node)
    : LayoutSVGShape(node), use_path_fallback_(false) {}

LayoutSVGRect::~LayoutSVGRect() {}

void LayoutSVGRect::UpdateShapeFromElement() {
  // Before creating a new object we need to clear the cached bounding box
  // to avoid using garbage.
  fill_bounding_box_ = FloatRect();
  stroke_bounding_box_ = FloatRect();
  use_path_fallback_ = false;
  SVGRectElement* rect = ToSVGRectElement(GetElement());
  DCHECK(rect);

  SVGLengthContext length_context(rect);
  FloatSize bounding_box_size(
      length_context.ValueForLength(StyleRef().Width(), StyleRef(),
                                    SVGLengthMode::kWidth),
      length_context.ValueForLength(StyleRef().Height(), StyleRef(),
                                    SVGLengthMode::kHeight));

  // Spec: "A negative value is an error."
  if (bounding_box_size.Width() < 0 || bounding_box_size.Height() < 0)
    return;

  // Spec: "A value of zero disables rendering of the element."
  if (!bounding_box_size.IsEmpty()) {
    // Fallback to LayoutSVGShape and path-based hit detection if the rect
    // has rounded corners or a non-scaling or non-simple stroke.
    // However, only use LayoutSVGShape bounding-box calculations for the
    // non-scaling stroke case, since the computation below should be accurate
    // for the other cases.
    if (HasNonScalingStroke()) {
      LayoutSVGShape::UpdateShapeFromElement();
      use_path_fallback_ = true;
      return;
    }
    if (length_context.ValueForLength(StyleRef().SvgStyle().Rx(), StyleRef(),
                                      SVGLengthMode::kWidth) > 0 ||
        length_context.ValueForLength(StyleRef().SvgStyle().Ry(), StyleRef(),
                                      SVGLengthMode::kHeight) > 0 ||
        !DefinitelyHasSimpleStroke()) {
      CreatePath();
      use_path_fallback_ = true;
    }
  }

  fill_bounding_box_ = FloatRect(
      FloatPoint(
          length_context.ValueForLength(StyleRef().SvgStyle().X(), StyleRef(),
                                        SVGLengthMode::kWidth),
          length_context.ValueForLength(StyleRef().SvgStyle().Y(), StyleRef(),
                                        SVGLengthMode::kHeight)),
      bounding_box_size);
  stroke_bounding_box_ = fill_bounding_box_;
  if (Style()->SvgStyle().HasStroke())
    stroke_bounding_box_.Inflate(StrokeWidth() / 2);
}

bool LayoutSVGRect::ShapeDependentStrokeContains(const FloatPoint& point) {
  // The optimized code below does not support non-simple strokes so we need
  // to fall back to LayoutSVGShape::shapeDependentStrokeContains in these
  // cases.
  if (use_path_fallback_ || !DefinitelyHasSimpleStroke()) {
    if (!HasPath())
      CreatePath();
    return LayoutSVGShape::ShapeDependentStrokeContains(point);
  }

  const float half_stroke_width = StrokeWidth() / 2;
  const float half_width = fill_bounding_box_.Width() / 2;
  const float half_height = fill_bounding_box_.Height() / 2;

  const FloatPoint fill_bounding_box_center =
      FloatPoint(fill_bounding_box_.X() + half_width,
                 fill_bounding_box_.Y() + half_height);
  const float abs_delta_x = std::abs(point.X() - fill_bounding_box_center.X());
  const float abs_delta_y = std::abs(point.Y() - fill_bounding_box_center.Y());

  if (!(abs_delta_x <= half_width + half_stroke_width &&
        abs_delta_y <= half_height + half_stroke_width))
    return false;

  return (half_width - half_stroke_width <= abs_delta_x) ||
         (half_height - half_stroke_width <= abs_delta_y);
}

bool LayoutSVGRect::ShapeDependentFillContains(const FloatPoint& point,
                                               const WindRule fill_rule) const {
  if (use_path_fallback_)
    return LayoutSVGShape::ShapeDependentFillContains(point, fill_rule);
  return fill_bounding_box_.Contains(point.X(), point.Y());
}

// Returns true if the stroke is continuous and definitely uses miter joins.
bool LayoutSVGRect::DefinitelyHasSimpleStroke() const {
  const SVGComputedStyle& svg_style = Style()->SvgStyle();

  // The four angles of a rect are 90 degrees. Using the formula at:
  // http://www.w3.org/TR/SVG/painting.html#StrokeMiterlimitProperty
  // when the join style of the rect is "miter", the ratio of the miterLength
  // to the stroke-width is found to be
  // miterLength / stroke-width = 1 / sin(45 degrees)
  //                            = 1 / (1 / sqrt(2))
  //                            = sqrt(2)
  //                            = 1.414213562373095...
  // When sqrt(2) exceeds the miterlimit, then the join style switches to
  // "bevel". When the miterlimit is greater than or equal to sqrt(2) then
  // the join style remains "miter".
  //
  // An approximation of sqrt(2) is used here because at certain precise
  // miterlimits, the join style used might not be correct (e.g. a miterlimit
  // of 1.4142135 should result in bevel joins, but may be drawn using miter
  // joins).
  return svg_style.StrokeDashArray()->IsEmpty() &&
         svg_style.JoinStyle() == kMiterJoin &&
         svg_style.StrokeMiterLimit() >= 1.5;
}

}  // namespace blink
