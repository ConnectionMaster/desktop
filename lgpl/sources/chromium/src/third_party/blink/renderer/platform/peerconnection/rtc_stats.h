// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_STATS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_STATS_H_

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_rtc_stats.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"
#include "third_party/webrtc/api/stats/rtc_stats_collector_callback.h"
#include "third_party/webrtc/api/stats/rtc_stats_report.h"

namespace blink {

// Wrapper around a webrtc::RTCStatsReport. Filters out any stats objects that
// aren't whitelisted. |filter| controls whether to include only standard
// members (RTCStatsMemberInterface::is_standardized return true) or not
// (RTCStatsMemberInterface::is_standardized return false).
//
// Note: This class is named |RTCStatsReportPlatform| not to collide with class
// |RTCStatsReport|, from renderer/modules/peerconnection/rtc_stats_report.cc|h.
//
// TODO(crbug.com/787254): Switch over the classes below from using WebVector
// and WebString to WTF::Vector and WTF::String, when their respective parent
// classes are gone.
class PLATFORM_EXPORT RTCStatsReportPlatform : public WebRTCStatsReport {
 public:
  RTCStatsReportPlatform(
      const scoped_refptr<const webrtc::RTCStatsReport>& stats_report,
      const blink::WebVector<webrtc::NonStandardGroupId>& exposed_group_ids);
  ~RTCStatsReportPlatform() override;
  std::unique_ptr<blink::WebRTCStatsReport> CopyHandle() const override;

  std::unique_ptr<blink::WebRTCStats> GetStats(
      blink::WebString id) const override;
  std::unique_ptr<blink::WebRTCStats> Next() override;
  size_t Size() const override;

 private:
  const scoped_refptr<const webrtc::RTCStatsReport> stats_report_;
  webrtc::RTCStatsReport::ConstIterator it_;
  const webrtc::RTCStatsReport::ConstIterator end_;
  blink::WebVector<webrtc::NonStandardGroupId> exposed_group_ids_;
  // Number of whitelisted webrtc::RTCStats in |stats_report_|.
  const size_t size_;
};

class PLATFORM_EXPORT RTCStats : public blink::WebRTCStats {
 public:
  RTCStats(
      const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
      const webrtc::RTCStats* stats,
      const blink::WebVector<webrtc::NonStandardGroupId>& exposed_group_ids);
  ~RTCStats() override;

  blink::WebString Id() const override;
  blink::WebString GetType() const override;
  double Timestamp() const override;

  size_t MembersCount() const override;
  std::unique_ptr<blink::WebRTCStatsMember> GetMember(size_t i) const override;

 private:
  // Reference to keep the report that owns |stats_| alive.
  const scoped_refptr<const webrtc::RTCStatsReport> stats_owner_;
  // Pointer to a stats object that is owned by |stats_owner_|.
  const webrtc::RTCStats* const stats_;
  // Members of the |stats_| object, equivalent to |stats_->Members()|.
  const std::vector<const webrtc::RTCStatsMemberInterface*> stats_members_;
};

class PLATFORM_EXPORT RTCStatsMember : public blink::WebRTCStatsMember {
 public:
  RTCStatsMember(const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
                 const webrtc::RTCStatsMemberInterface* member);
  ~RTCStatsMember() override;

  blink::WebString GetName() const override;
  webrtc::RTCStatsMemberInterface::Type GetType() const override;
  bool IsDefined() const override;

  bool ValueBool() const override;
  int32_t ValueInt32() const override;
  uint32_t ValueUint32() const override;
  int64_t ValueInt64() const override;
  uint64_t ValueUint64() const override;
  double ValueDouble() const override;
  blink::WebString ValueString() const override;
  blink::WebVector<int> ValueSequenceBool() const override;
  blink::WebVector<int32_t> ValueSequenceInt32() const override;
  blink::WebVector<uint32_t> ValueSequenceUint32() const override;
  blink::WebVector<int64_t> ValueSequenceInt64() const override;
  blink::WebVector<uint64_t> ValueSequenceUint64() const override;
  blink::WebVector<double> ValueSequenceDouble() const override;
  blink::WebVector<blink::WebString> ValueSequenceString() const override;

 private:
  // Reference to keep the report that owns |member_|'s stats object alive.
  const scoped_refptr<const webrtc::RTCStatsReport> stats_owner_;
  // Pointer to member of a stats object that is owned by |stats_owner_|.
  const webrtc::RTCStatsMemberInterface* const member_;
};

// A stats collector callback.
// It is invoked on the WebRTC signaling thread and will post a task to invoke
// |callback| on the thread given in the |main_thread| argument.
// The argument to the callback will be a |blink::WebRTCStatsReport|.
class PLATFORM_EXPORT RTCStatsCollectorCallbackImpl
    : public webrtc::RTCStatsCollectorCallback {
 public:
  void OnStatsDelivered(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;

 protected:
  RTCStatsCollectorCallbackImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread,
      blink::WebRTCStatsReportCallback callback2,
      const blink::WebVector<webrtc::NonStandardGroupId>& exposed_group_ids);
  ~RTCStatsCollectorCallbackImpl() override;

  void OnStatsDeliveredOnMainThread(
      rtc::scoped_refptr<const webrtc::RTCStatsReport> report);

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  blink::WebRTCStatsReportCallback callback_;
  blink::WebVector<webrtc::NonStandardGroupId> exposed_group_ids_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_STATS_H_
