/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebMediaConstraints_h
#define WebMediaConstraints_h

#include <string>
#include <vector>

#include "WebCommon.h"
#include "WebPrivatePtr.h"
#include "WebString.h"
#include "WebVector.h"

namespace blink {

class WebMediaConstraintsPrivate;

class BLINK_PLATFORM_EXPORT BaseConstraint {
 public:
  explicit BaseConstraint(const char* name);
  virtual ~BaseConstraint();
  virtual bool IsEmpty() const = 0;
  virtual bool HasMandatory() const = 0;
  const char* GetName() const { return name_; }
  virtual WebString ToString() const = 0;

 private:
  const char* name_;
};

class BLINK_PLATFORM_EXPORT LongConstraint : public BaseConstraint {
 public:
  explicit LongConstraint(const char* name);

  void SetMin(long value) {
    min_ = value;
    has_min_ = true;
  }

  void SetMax(long value) {
    max_ = value;
    has_max_ = true;
  }

  void SetExact(long value) {
    exact_ = value;
    has_exact_ = true;
  }

  void SetIdeal(long value) {
    ideal_ = value;
    has_ideal_ = true;
  }

  bool Matches(long value) const;
  bool IsEmpty() const override;
  bool HasMandatory() const override;
  WebString ToString() const override;
  bool HasMin() const { return has_min_; }
  long Min() const { return min_; }
  bool HasMax() const { return has_max_; }
  long Max() const { return max_; }
  bool HasExact() const { return has_exact_; }
  long Exact() const { return exact_; }
  bool HasIdeal() const { return has_ideal_; }
  long Ideal() const { return ideal_; }

 private:
  long min_;
  long max_;
  long exact_;
  long ideal_;
  unsigned has_min_ : 1;
  unsigned has_max_ : 1;
  unsigned has_exact_ : 1;
  unsigned has_ideal_ : 1;
};

class BLINK_PLATFORM_EXPORT DoubleConstraint : public BaseConstraint {
 public:
  // Permit a certain leeway when comparing floats. The offset of 0.00001
  // is chosen based on observed behavior of doubles formatted with
  // rtc::ToString.
  static const double kConstraintEpsilon;

  explicit DoubleConstraint(const char* name);

  void SetMin(double value) {
    min_ = value;
    has_min_ = true;
  }

  void SetMax(double value) {
    max_ = value;
    has_max_ = true;
  }

  void SetExact(double value) {
    exact_ = value;
    has_exact_ = true;
  }

  void SetIdeal(double value) {
    ideal_ = value;
    has_ideal_ = true;
  }

  bool Matches(double value) const;
  bool IsEmpty() const override;
  bool HasMandatory() const override;
  WebString ToString() const override;
  bool HasMin() const { return has_min_; }
  double Min() const { return min_; }
  bool HasMax() const { return has_max_; }
  double Max() const { return max_; }
  bool HasExact() const { return has_exact_; }
  double Exact() const { return exact_; }
  bool HasIdeal() const { return has_ideal_; }
  double Ideal() const { return ideal_; }

 private:
  double min_;
  double max_;
  double exact_;
  double ideal_;
  unsigned has_min_ : 1;
  unsigned has_max_ : 1;
  unsigned has_exact_ : 1;
  unsigned has_ideal_ : 1;
};

class BLINK_PLATFORM_EXPORT StringConstraint : public BaseConstraint {
 public:
  // String-valued options don't have min or max, but can have multiple
  // values for ideal and exact.
  explicit StringConstraint(const char* name);

  void SetExact(const WebString& exact) { exact_.Assign(&exact, 1); }

  void SetExact(const WebVector<WebString>& exact) { exact_.Assign(exact); }

  void SetIdeal(const WebString& ideal) { ideal_.Assign(&ideal, 1); }

  void SetIdeal(const WebVector<WebString>& ideal) { ideal_.Assign(ideal); }

  bool Matches(WebString value) const;
  bool IsEmpty() const override;
  bool HasMandatory() const override;
  WebString ToString() const override;
  bool HasExact() const { return !exact_.empty(); }
  bool HasIdeal() const { return !ideal_.empty(); }
  const WebVector<WebString>& Exact() const;
  const WebVector<WebString>& Ideal() const;

 private:
  WebVector<WebString> exact_;
  WebVector<WebString> ideal_;
};

class BLINK_PLATFORM_EXPORT BooleanConstraint : public BaseConstraint {
 public:
  explicit BooleanConstraint(const char* name);

  bool Exact() const { return exact_; }
  bool Ideal() const { return ideal_; }
  void SetIdeal(bool value) {
    ideal_ = value;
    has_ideal_ = true;
  }

  void SetExact(bool value) {
    exact_ = value;
    has_exact_ = true;
  }

  bool Matches(bool value) const;
  bool IsEmpty() const override;
  bool HasMandatory() const override;
  WebString ToString() const override;
  bool HasExact() const { return has_exact_; }
  bool HasIdeal() const { return has_ideal_; }

 private:
  unsigned ideal_ : 1;
  unsigned exact_ : 1;
  unsigned has_ideal_ : 1;
  unsigned has_exact_ : 1;
};

struct WebMediaTrackConstraintSet {
 public:
  BLINK_PLATFORM_EXPORT WebMediaTrackConstraintSet();

  LongConstraint width;
  LongConstraint height;
  DoubleConstraint aspect_ratio;
  DoubleConstraint frame_rate;
  StringConstraint facing_mode;
  DoubleConstraint volume;
  LongConstraint sample_rate;
  LongConstraint sample_size;
  BooleanConstraint echo_cancellation;
  DoubleConstraint latency;
  LongConstraint channel_count;
  StringConstraint device_id;
  BooleanConstraint disable_local_echo;
  StringConstraint group_id;
  // https://w3c.github.io/mediacapture-depth/#mediatrackconstraints
  StringConstraint video_kind;
  DoubleConstraint depth_near;
  DoubleConstraint depth_far;
  DoubleConstraint focal_length_x;
  DoubleConstraint focal_length_y;
  // Constraints not exposed in Blink at the moment, only through
  // the legacy name interface.
  StringConstraint media_stream_source;  // tab, screen, desktop, system
  BooleanConstraint render_to_associated_sink;
  BooleanConstraint hotword_enabled;
  BooleanConstraint goog_echo_cancellation;
  BooleanConstraint goog_experimental_echo_cancellation;
  BooleanConstraint goog_auto_gain_control;
  BooleanConstraint goog_experimental_auto_gain_control;
  BooleanConstraint goog_noise_suppression;
  BooleanConstraint goog_highpass_filter;
  BooleanConstraint goog_typing_noise_detection;
  BooleanConstraint goog_experimental_noise_suppression;
  BooleanConstraint goog_beamforming;
  StringConstraint goog_array_geometry;
  BooleanConstraint goog_audio_mirroring;
  BooleanConstraint goog_da_echo_cancellation;
  BooleanConstraint goog_noise_reduction;
  LongConstraint offer_to_receive_audio;
  LongConstraint offer_to_receive_video;
  BooleanConstraint voice_activity_detection;
  BooleanConstraint ice_restart;
  BooleanConstraint goog_use_rtp_mux;
  BooleanConstraint enable_dtls_srtp;
  BooleanConstraint enable_rtp_data_channels;
  BooleanConstraint enable_dscp;
  BooleanConstraint enable_i_pv6;
  BooleanConstraint goog_enable_video_suspend_below_min_bitrate;
  LongConstraint goog_num_unsignalled_recv_streams;
  BooleanConstraint goog_combined_audio_video_bwe;
  LongConstraint goog_screencast_min_bitrate;
  BooleanConstraint goog_cpu_overuse_detection;
  LongConstraint goog_cpu_underuse_threshold;
  LongConstraint goog_cpu_overuse_threshold;
  LongConstraint goog_cpu_underuse_encode_rsd_threshold;
  LongConstraint goog_cpu_overuse_encode_rsd_threshold;
  BooleanConstraint goog_cpu_overuse_encode_usage;
  LongConstraint goog_high_start_bitrate;
  BooleanConstraint goog_payload_padding;
  LongConstraint goog_latency_ms;
  LongConstraint goog_power_line_frequency;

  BLINK_PLATFORM_EXPORT bool IsEmpty() const;
  BLINK_PLATFORM_EXPORT bool HasMandatory() const;
  BLINK_PLATFORM_EXPORT bool HasMandatoryOutsideSet(
      const std::vector<std::string>&,
      std::string&) const;
  BLINK_PLATFORM_EXPORT WebString ToString() const;

 private:
  std::vector<const BaseConstraint*> AllConstraints() const;
};

class WebMediaConstraints {
 public:
  WebMediaConstraints() {}
  WebMediaConstraints(const WebMediaConstraints& other) { Assign(other); }
  ~WebMediaConstraints() { Reset(); }

  WebMediaConstraints& operator=(const WebMediaConstraints& other) {
    Assign(other);
    return *this;
  }

  BLINK_PLATFORM_EXPORT void Assign(const WebMediaConstraints&);

  BLINK_PLATFORM_EXPORT void Reset();
  bool IsNull() const { return private_.IsNull(); }
  BLINK_PLATFORM_EXPORT bool IsEmpty() const;

  BLINK_PLATFORM_EXPORT void Initialize();
  BLINK_PLATFORM_EXPORT void Initialize(
      const WebMediaTrackConstraintSet& basic,
      const WebVector<WebMediaTrackConstraintSet>& advanced);

  BLINK_PLATFORM_EXPORT const WebMediaTrackConstraintSet& Basic() const;
  BLINK_PLATFORM_EXPORT const WebVector<WebMediaTrackConstraintSet>& Advanced()
      const;

  BLINK_PLATFORM_EXPORT const WebString ToString() const;

 private:
  WebPrivatePtr<WebMediaConstraintsPrivate> private_;
};

}  // namespace blink

#endif  // WebMediaConstraints_h
