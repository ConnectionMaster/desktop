// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSMathSum.h"

namespace blink {

namespace {

CSSNumericValueType NumericTypeFromUnitMap(
    const CSSNumericSumValue::UnitMap& units) {
  CSSNumericValueType type;
  for (const auto& unit_exponent : units) {
    bool error = false;
    type = CSSNumericValueType::Multiply(
        type, CSSNumericValueType(unit_exponent.value, unit_exponent.key),
        error);
    DCHECK(!error);
  }
  return type;
}

bool CanCreateNumericTypeFromSumValue(const CSSNumericSumValue& sum) {
  DCHECK(!sum.terms.IsEmpty());

  const auto first_type = NumericTypeFromUnitMap(sum.terms[0].units);
  return std::all_of(sum.terms.begin(), sum.terms.end(),
                     [&first_type](const CSSNumericSumValue::Term& term) {
                       bool error = false;
                       CSSNumericValueType::Add(
                           first_type, NumericTypeFromUnitMap(term.units),
                           error);
                       return !error;
                     });
}

struct UnitMapComparator {
  CSSNumericSumValue::Term term;
};

bool operator==(const CSSNumericSumValue::Term& a, const UnitMapComparator& b) {
  return a.units == b.term.units;
}

}  // namespace

CSSMathSum* CSSMathSum::Create(const HeapVector<CSSNumberish>& args,
                               ExceptionState& exception_state) {
  if (args.IsEmpty()) {
    exception_state.ThrowDOMException(kSyntaxError, "Arguments can't be empty");
    return nullptr;
  }

  CSSMathSum* result = Create(CSSNumberishesToNumericValues(args));
  if (!result) {
    exception_state.ThrowTypeError("Incompatible types");
    return nullptr;
  }

  return result;
}

CSSMathSum* CSSMathSum::Create(CSSNumericValueVector values) {
  bool error = false;
  CSSNumericValueType final_type =
      CSSMathVariadic::TypeCheck(values, CSSNumericValueType::Add, error);
  return error ? nullptr
               : new CSSMathSum(CSSNumericArray::Create(std::move(values)),
                                final_type);
}

WTF::Optional<CSSNumericSumValue> CSSMathSum::SumValue() const {
  CSSNumericSumValue sum;
  for (const auto& value : NumericValues()) {
    const auto child_sum = value->SumValue();
    if (!child_sum)
      return WTF::nullopt;

    // Collect like-terms
    for (const auto& term : child_sum->terms) {
      size_t index = sum.terms.Find(UnitMapComparator{term});
      if (index == kNotFound)
        sum.terms.push_back(term);
      else
        sum.terms[index].value += term.value;
    }
  }

  if (!CanCreateNumericTypeFromSumValue(sum))
    return WTF::nullopt;

  return sum;
}

}  // namespace blink
