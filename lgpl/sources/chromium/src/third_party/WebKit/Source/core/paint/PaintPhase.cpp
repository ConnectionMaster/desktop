// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPhase.h"

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/wtf/Assertions.h"

namespace blink {

static_assert(static_cast<PaintPhase>(DisplayItem::kPaintPhaseMax) ==
                  PaintPhase::kMax,
              "DisplayItem Type and PaintPhase should stay in sync");

}  // namespace blink
