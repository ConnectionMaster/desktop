// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFontFaceSource_h
#define RemoteFontFaceSource_h

#include "core/css/CSSFontFaceSource.h"
#include "core/loader/resource/FontResource.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSFontFace;
class FontSelector;
class FontCustomPlatformData;

enum FontDisplay {
  kFontDisplayAuto,
  kFontDisplayBlock,
  kFontDisplaySwap,
  kFontDisplayFallback,
  kFontDisplayOptional,
  kFontDisplayEnumMax
};

class RemoteFontFaceSource final : public CSSFontFaceSource,
                                   public FontResourceClient {
  USING_PRE_FINALIZER(RemoteFontFaceSource, Dispose);
  USING_GARBAGE_COLLECTED_MIXIN(RemoteFontFaceSource);

 public:
  enum DisplayPeriod { kBlockPeriod, kSwapPeriod, kFailurePeriod };

  RemoteFontFaceSource(CSSFontFace*, FontResource*, FontSelector*, FontDisplay);
  ~RemoteFontFaceSource() override;
  void Dispose();

  bool IsLoading() const override;
  bool IsLoaded() const override;
  bool IsValid() const override;

  void BeginLoadIfNeeded() override;

  void NotifyFinished(Resource*) override;
  void FontLoadShortLimitExceeded(FontResource*) override;
  void FontLoadLongLimitExceeded(FontResource*) override;
  String DebugName() const override { return "RemoteFontFaceSource"; }

  bool IsInBlockPeriod() const override { return period_ == kBlockPeriod; }
  bool IsInFailurePeriod() const override { return period_ == kFailurePeriod; }

  // For UMA reporting
  bool HadBlankText() override { return histograms_.HadBlankText(); }
  void PaintRequested() { histograms_.FallbackFontPainted(period_); }

  virtual void Trace(blink::Visitor*);

 protected:
  scoped_refptr<SimpleFontData> CreateFontData(
      const FontDescription&,
      const FontSelectionCapabilities&) override;
  scoped_refptr<SimpleFontData> CreateLoadingFallbackFontData(
      const FontDescription&);

 private:
  class FontLoadHistograms {
    DISALLOW_NEW();

   public:
    // Should not change following order in CacheHitMetrics to be used for
    // metrics values.
    enum CacheHitMetrics {
      kMiss,
      kDiskHit,
      kDataUrl,
      kMemoryHit,
      kCacheHitEnumMax
    };
    enum DataSource {
      kFromUnknown,
      kFromDataURL,
      kFromMemoryCache,
      kFromDiskCache,
      kFromNetwork
    };

    FontLoadHistograms(DataSource data_source)
        : load_start_time_(0),
          blank_paint_time_(0),
          is_long_limit_exceeded_(false),
          data_source_(data_source) {}
    void LoadStarted();
    void FallbackFontPainted(DisplayPeriod);
    void LongLimitExceeded();
    void RecordFallbackTime();
    void RecordRemoteFont(const FontResource*);
    bool HadBlankText() { return blank_paint_time_; }
    DataSource GetDataSource() { return data_source_; }
    void MaySetDataSource(DataSource);

   private:
    void RecordLoadTimeHistogram(const FontResource*, int duration);
    CacheHitMetrics DataSourceMetricsValue();
    double load_start_time_;
    double blank_paint_time_;
    bool is_long_limit_exceeded_;
    DataSource data_source_;
  };

  void SwitchToSwapPeriod();
  void SwitchToFailurePeriod();
  bool ShouldTriggerWebFontsIntervention();
  bool IsLowPriorityLoadingAllowedForRemoteFont() const override;

  // Our owning font face.
  Member<CSSFontFace> face_;
  // Cleared once load is finished.
  Member<FontResource> font_;
  Member<FontSelector> font_selector_;

  // |nullptr| if font is not loaded or failed to decode.
  scoped_refptr<FontCustomPlatformData> custom_font_data_;

  const FontDisplay display_;
  DisplayPeriod period_;
  FontLoadHistograms histograms_;
  bool is_intervention_triggered_;
};

}  // namespace blink

#endif
