// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintRecordBuilder_h
#define PaintRecordBuilder_h

#include <memory>

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItemCacheSkipper.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/wtf/Noncopyable.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkMetaData;

namespace blink {

class GraphicsContext;
class PaintController;

// TODO(enne): rename this class to not be named SkPicture
// When slimming paint ships we can remove this PaintRecord abstraction and
// rely on PaintController here.
class PLATFORM_EXPORT PaintRecordBuilder final : public DisplayItemClient {
  WTF_MAKE_NONCOPYABLE(PaintRecordBuilder);

 public:
  // Constructs a new builder for the resulting recorded picture. If |metadata|
  // is specified, that metadata is propagated to the builder's internal canvas.
  // If |containing_context| is specified, the device scale factor, printing,
  // and disabled state are propagated to the builder's internal context.
  // If a PaintController is passed, it is used as the PaintController for
  // painting the picture (and hence we can use its cache). Otherwise, a new
  // PaintController is used for the duration of the picture building, which
  // therefore has no caching.
  // In SPv175+ mode, resets paint chunks to PropertyTreeState::Root()
  // before beginning to record.
  PaintRecordBuilder(SkMetaData* = nullptr,
                     GraphicsContext* containing_context = nullptr,
                     PaintController* = nullptr);

  GraphicsContext& Context() { return *context_; }

  // Returns a PaintRecord capturing all drawing performed on the builder's
  // context since construction.
  // In SPv175+ mode, replays into the ancestor state given by |replay_state|.
  sk_sp<PaintRecord> EndRecording(
      const PropertyTreeState& replay_state = PropertyTreeState::Root());

  // Replays the recording directly into the given canvas.
  // In SPv175+ mode, replays into the ancestor state given by |replay_state|.
  void EndRecording(
      PaintCanvas&,
      const PropertyTreeState& replay_state = PropertyTreeState::Root());

  // DisplayItemClient methods
  String DebugName() const final { return "PaintRecordBuilder"; }
  LayoutRect VisualRect() const final { return LayoutRect(); }

 private:
  PaintController* paint_controller_;
  std::unique_ptr<PaintController> own_paint_controller_;
  std::unique_ptr<GraphicsContext> context_;
  Optional<DisplayItemCacheSkipper> cache_skipper_;
};

}  // namespace blink

#endif  // PaintRecordBuilder_h
