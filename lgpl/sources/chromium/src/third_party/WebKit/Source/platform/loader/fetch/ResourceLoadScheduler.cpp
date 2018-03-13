// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/ResourceLoadScheduler.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "platform/Histogram.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

namespace {

// Field trial name.
const char kResourceLoadSchedulerTrial[] = "ResourceLoadScheduler";

// Field trial parameter names.
// Note: bg_limit is supported on m61+, but bg_sub_limit is only on m63+.
// If bg_sub_limit param is not found, we should use bg_limit to make the
// study result statistically correct.
const char kOutstandingLimitForBackgroundMainFrameName[] = "bg_limit";
const char kOutstandingLimitForBackgroundSubFrameName[] = "bg_sub_limit";

// Field trial default parameters.
constexpr size_t kOutstandingLimitForBackgroundFrameDefault = 16u;

// Maximum request count that request count metrics assume.
constexpr base::HistogramBase::Sample kMaximumReportSize10K = 10000;

// Maximum traffic bytes that traffic metrics assume.
constexpr base::HistogramBase::Sample kMaximumReportSize1G =
    1 * 1000 * 1000 * 1000;

// Bucket count for metrics.
constexpr int32_t kReportBucketCount = 25;

// Represents a resource load circumstance, e.g. from main frame vs sub-frames,
// or on throttled state vs on not-throttled state.
// Used to report histograms. Do not reorder or insert new items.
enum class ReportCircumstance {
  kMainframeThrottled,
  kMainframeNotThrottled,
  kSubframeThrottled,
  kSubframeNotThrottled,
  // Append new items here.
  kNumOfCircumstances,
};

base::HistogramBase::Sample ToSample(ReportCircumstance circumstance) {
  return static_cast<base::HistogramBase::Sample>(circumstance);
}

uint32_t GetFieldTrialUint32Param(const char* name, uint32_t default_param) {
  std::map<std::string, std::string> trial_params;
  bool result =
      base::GetFieldTrialParams(kResourceLoadSchedulerTrial, &trial_params);
  if (!result)
    return default_param;

  const auto& found = trial_params.find(name);
  if (found == trial_params.end())
    return default_param;

  uint32_t param;
  if (!base::StringToUint(found->second, &param))
    return default_param;

  return param;
}

uint32_t GetOutstandingThrottledLimit(FetchContext* context) {
  DCHECK(context);

  uint32_t main_frame_limit =
      GetFieldTrialUint32Param(kOutstandingLimitForBackgroundMainFrameName,
                               kOutstandingLimitForBackgroundFrameDefault);
  if (context->IsMainFrame())
    return main_frame_limit;

  // We do not have a fixed default limit for sub-frames, but use the limit for
  // the main frame so that it works as how previous versions that haven't
  // consider sub-frames' specific limit work.
  return GetFieldTrialUint32Param(kOutstandingLimitForBackgroundSubFrameName,
                                  main_frame_limit);
}

}  // namespace

// A class to gather throttling and traffic information to report histograms.
class ResourceLoadScheduler::TrafficMonitor {
 public:
  explicit TrafficMonitor(bool is_main_frame);
  ~TrafficMonitor();

  // Notified when the ThrottlingState is changed.
  void OnThrottlingStateChanged(WebFrameScheduler::ThrottlingState);

  // Reports resource request completion.
  void Report(const ResourceLoadScheduler::TrafficReportHints&);

  // Reports per-frame reports.
  void ReportAll();

 private:
  const bool is_main_frame_;

  WebFrameScheduler::ThrottlingState current_state_ =
      WebFrameScheduler::ThrottlingState::kStopped;

  size_t total_throttled_request_count_ = 0;
  size_t total_throttled_traffic_bytes_ = 0;
  size_t total_throttled_decoded_bytes_ = 0;
  size_t total_not_throttled_request_count_ = 0;
  size_t total_not_throttled_traffic_bytes_ = 0;
  size_t total_not_throttled_decoded_bytes_ = 0;
  size_t throttling_state_change_count_ = 0;
  bool report_all_is_called_ = false;
};

ResourceLoadScheduler::TrafficMonitor::TrafficMonitor(bool is_main_frame)
    : is_main_frame_(is_main_frame) {}

ResourceLoadScheduler::TrafficMonitor::~TrafficMonitor() {
  ReportAll();
}

void ResourceLoadScheduler::TrafficMonitor::OnThrottlingStateChanged(
    WebFrameScheduler::ThrottlingState state) {
  current_state_ = state;
  throttling_state_change_count_++;
}

void ResourceLoadScheduler::TrafficMonitor::Report(
    const ResourceLoadScheduler::TrafficReportHints& hints) {
  // Currently we only care about stats from frames.
  if (!IsMainThread())
    return;
  if (!hints.IsValid())
    return;

  DEFINE_STATIC_LOCAL(EnumerationHistogram, request_count_by_circumstance,
                      ("Blink.ResourceLoadScheduler.RequestCount",
                       ToSample(ReportCircumstance::kNumOfCircumstances)));

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_throttled_traffic_bytes,
      ("Blink.ResourceLoadScheduler.TrafficBytes.MainframeThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_not_throttled_traffic_bytes,
      ("Blink.ResourceLoadScheduler.TrafficBytes.MainframeNotThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_throttled_traffic_bytes,
      ("Blink.ResourceLoadScheduler.TrafficBytes.SubframeThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_not_throttled_traffic_bytes,
      ("Blink.ResourceLoadScheduler.TrafficBytes.SubframeNotThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_throttled_decoded_bytes,
      ("Blink.ResourceLoadScheduler.DecodedBytes.MainframeThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_not_throttled_decoded_bytes,
      ("Blink.ResourceLoadScheduler.DecodedBytes.MainframeNotThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_throttled_decoded_bytes,
      ("Blink.ResourceLoadScheduler.DecodedBytes.SubframeThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_not_throttled_decoded_bytes,
      ("Blink.ResourceLoadScheduler.DecodedBytes.SubframeNotThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));

  switch (current_state_) {
    case WebFrameScheduler::ThrottlingState::kThrottled:
      if (is_main_frame_) {
        request_count_by_circumstance.Count(
            ToSample(ReportCircumstance::kMainframeThrottled));
        main_frame_throttled_traffic_bytes.Count(hints.encoded_data_length());
        main_frame_throttled_decoded_bytes.Count(hints.decoded_body_length());
      } else {
        request_count_by_circumstance.Count(
            ToSample(ReportCircumstance::kSubframeThrottled));
        sub_frame_throttled_traffic_bytes.Count(hints.encoded_data_length());
        sub_frame_throttled_decoded_bytes.Count(hints.decoded_body_length());
      }
      total_throttled_request_count_++;
      total_throttled_traffic_bytes_ += hints.encoded_data_length();
      total_throttled_decoded_bytes_ += hints.decoded_body_length();
      break;
    case WebFrameScheduler::ThrottlingState::kNotThrottled:
      if (is_main_frame_) {
        request_count_by_circumstance.Count(
            ToSample(ReportCircumstance::kMainframeNotThrottled));
        main_frame_not_throttled_traffic_bytes.Count(
            hints.encoded_data_length());
        main_frame_not_throttled_decoded_bytes.Count(
            hints.decoded_body_length());
      } else {
        request_count_by_circumstance.Count(
            ToSample(ReportCircumstance::kSubframeNotThrottled));
        sub_frame_not_throttled_traffic_bytes.Count(
            hints.encoded_data_length());
        sub_frame_not_throttled_decoded_bytes.Count(
            hints.decoded_body_length());
      }
      total_not_throttled_request_count_++;
      total_not_throttled_traffic_bytes_ += hints.encoded_data_length();
      total_not_throttled_decoded_bytes_ += hints.decoded_body_length();
      break;
    case WebFrameScheduler::ThrottlingState::kStopped:
      break;
  }
}

void ResourceLoadScheduler::TrafficMonitor::ReportAll() {
  // Currently we only care about stats from frames.
  if (!IsMainThread())
    return;
  if (report_all_is_called_)
    return;
  report_all_is_called_ = true;

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_total_throttled_request_count,
      ("Blink.ResourceLoadScheduler.TotalRequestCount.MainframeThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_total_not_throttled_request_count,
      ("Blink.ResourceLoadScheduler.TotalRequestCount.MainframeNotThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_total_throttled_request_count,
      ("Blink.ResourceLoadScheduler.TotalRequestCount.SubframeThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_total_not_throttled_request_count,
      ("Blink.ResourceLoadScheduler.TotalRequestCount.SubframeNotThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_total_throttled_traffic_bytes,
      ("Blink.ResourceLoadScheduler.TotalTrafficBytes.MainframeThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_total_not_throttled_traffic_bytes,
      ("Blink.ResourceLoadScheduler.TotalTrafficBytes.MainframeNotThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_total_throttled_traffic_bytes,
      ("Blink.ResourceLoadScheduler.TotalTrafficBytes.SubframeThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_total_not_throttled_traffic_bytes,
      ("Blink.ResourceLoadScheduler.TotalTrafficBytes.SubframeNotThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_total_throttled_decoded_bytes,
      ("Blink.ResourceLoadScheduler.TotalDecodedBytes.MainframeThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_total_not_throttled_decoded_bytes,
      ("Blink.ResourceLoadScheduler.TotalDecodedBytes.MainframeNotThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_total_throttled_decoded_bytes,
      ("Blink.ResourceLoadScheduler.TotalDecodedBytes.SubframeThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_total_not_throttled_decoded_bytes,
      ("Blink.ResourceLoadScheduler.TotalDecodedBytes.SubframeNotThrottled", 0,
       kMaximumReportSize1G, kReportBucketCount));

  DEFINE_STATIC_LOCAL(CustomCountHistogram, throttling_state_change_count,
                      ("Blink.ResourceLoadScheduler.ThrottlingStateChangeCount",
                       0, 100, kReportBucketCount));

  if (is_main_frame_) {
    main_frame_total_throttled_request_count.Count(
        total_throttled_request_count_);
    main_frame_total_not_throttled_request_count.Count(
        total_not_throttled_request_count_);
    main_frame_total_throttled_traffic_bytes.Count(
        total_throttled_traffic_bytes_);
    main_frame_total_not_throttled_traffic_bytes.Count(
        total_not_throttled_traffic_bytes_);
    main_frame_total_throttled_decoded_bytes.Count(
        total_throttled_decoded_bytes_);
    main_frame_total_not_throttled_decoded_bytes.Count(
        total_not_throttled_decoded_bytes_);
  } else {
    sub_frame_total_throttled_request_count.Count(
        total_throttled_request_count_);
    sub_frame_total_not_throttled_request_count.Count(
        total_not_throttled_request_count_);
    sub_frame_total_throttled_traffic_bytes.Count(
        total_throttled_traffic_bytes_);
    sub_frame_total_not_throttled_traffic_bytes.Count(
        total_not_throttled_traffic_bytes_);
    sub_frame_total_throttled_decoded_bytes.Count(
        total_throttled_decoded_bytes_);
    sub_frame_total_not_throttled_decoded_bytes.Count(
        total_not_throttled_decoded_bytes_);
  }

  throttling_state_change_count.Count(throttling_state_change_count_);
}

ResourceLoadScheduler::TrafficReportHints&
ResourceLoadScheduler::TrafficReportHints::InvalidInstance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(TrafficReportHints, instance, ());
  return instance;
}

constexpr ResourceLoadScheduler::ClientId
    ResourceLoadScheduler::kInvalidClientId;

ResourceLoadScheduler::ResourceLoadScheduler(FetchContext* context)
    : outstanding_throttled_limit_(GetOutstandingThrottledLimit(context)),
      context_(context) {
  traffic_monitor_ = std::make_unique<ResourceLoadScheduler::TrafficMonitor>(
      context_->IsMainFrame());

  if (!RuntimeEnabledFeatures::ResourceLoadSchedulerEnabled())
    return;

  auto* scheduler = context->GetFrameScheduler();
  if (!scheduler)
    return;

  is_enabled_ = true;
  scheduler->AddThrottlingObserver(WebFrameScheduler::ObserverType::kLoader,
                                   this);
}

ResourceLoadScheduler::~ResourceLoadScheduler() = default;

void ResourceLoadScheduler::Trace(blink::Visitor* visitor) {
  visitor->Trace(pending_request_map_);
  visitor->Trace(context_);
}

void ResourceLoadScheduler::Shutdown() {
  // Do nothing if the feature is not enabled, or Shutdown() was already called.
  if (is_shutdown_)
    return;
  is_shutdown_ = true;

  if (traffic_monitor_)
    traffic_monitor_.reset();

  if (!is_enabled_)
    return;
  auto* scheduler = context_->GetFrameScheduler();
  DCHECK(scheduler);
  scheduler->RemoveThrottlingObserver(WebFrameScheduler::ObserverType::kLoader,
                                      this);
}

void ResourceLoadScheduler::Request(ResourceLoadSchedulerClient* client,
                                    ThrottleOption option,
                                    ResourceLoadPriority priority,
                                    int intra_priority,
                                    ResourceLoadScheduler::ClientId* id) {
  *id = GenerateClientId();
  if (is_shutdown_)
    return;

  if (!Platform::Current()->IsRendererSideResourceSchedulerEnabled()) {
    // Prioritization is effectively disabled as we use the constant priority.
    priority = ResourceLoadPriority::kMedium;
    intra_priority = 0;
  }

  if (!is_enabled_ || option == ThrottleOption::kCanNotBeThrottled ||
      !IsThrottablePriority(priority)) {
    Run(*id, client);
    return;
  }

  pending_requests_.emplace(*id, priority, intra_priority);
  pending_request_map_.insert(
      *id, new ClientWithPriority(client, priority, intra_priority));
  MaybeRun();
}

void ResourceLoadScheduler::SetPriority(ClientId client_id,
                                        ResourceLoadPriority priority,
                                        int intra_priority) {
  if (!Platform::Current()->IsRendererSideResourceSchedulerEnabled())
    return;

  auto client_it = pending_request_map_.find(client_id);
  if (client_it == pending_request_map_.end())
    return;

  auto it = pending_requests_.find(ClientIdWithPriority(
      client_id, client_it->value->priority, client_it->value->intra_priority));

  DCHECK(it != pending_requests_.end());
  pending_requests_.erase(it);

  client_it->value->priority = priority;
  client_it->value->intra_priority = intra_priority;

  if (!IsThrottablePriority(priority)) {
    ResourceLoadSchedulerClient* client = client_it->value->client;
    pending_request_map_.erase(client_it);
    Run(client_id, client);
    return;
  }
  pending_requests_.emplace(client_id, priority, intra_priority);
  MaybeRun();
}

bool ResourceLoadScheduler::Release(
    ResourceLoadScheduler::ClientId id,
    ResourceLoadScheduler::ReleaseOption option,
    const ResourceLoadScheduler::TrafficReportHints& hints) {
  // Check kInvalidClientId that can not be passed to the HashSet.
  if (id == kInvalidClientId)
    return false;

  if (running_requests_.find(id) != running_requests_.end()) {
    running_requests_.erase(id);

    if (traffic_monitor_)
      traffic_monitor_->Report(hints);

    if (option == ReleaseOption::kReleaseAndSchedule)
      MaybeRun();
    return true;
  }
  auto found = pending_request_map_.find(id);
  if (found != pending_request_map_.end()) {
    pending_request_map_.erase(found);
    // Intentionally does not remove it from |pending_requests_|.

    // Didn't release any running requests, but the outstanding limit might be
    // changed to allow another request.
    if (option == ReleaseOption::kReleaseAndSchedule)
      MaybeRun();
    return true;
  }
  return false;
}

void ResourceLoadScheduler::SetOutstandingLimitForTesting(size_t limit) {
  SetOutstandingLimitAndMaybeRun(limit);
}

void ResourceLoadScheduler::OnNetworkQuiet() {
  DCHECK(IsMainThread());

  // Flush out all traffic reports here for safety.
  traffic_monitor_->ReportAll();

  if (maximum_running_requests_seen_ == 0)
    return;

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_throttled,
      ("Blink.ResourceLoadScheduler.PeakRequests.MainframeThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_not_throttled,
      ("Blink.ResourceLoadScheduler.PeakRequests.MainframeNotThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, main_frame_partially_throttled,
      ("Blink.ResourceLoadScheduler.PeakRequests.MainframePartiallyThrottled",
       0, kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_throttled,
      ("Blink.ResourceLoadScheduler.PeakRequests.SubframeThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_not_throttled,
      ("Blink.ResourceLoadScheduler.PeakRequests.SubframeNotThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, sub_frame_partially_throttled,
      ("Blink.ResourceLoadScheduler.PeakRequests.SubframePartiallyThrottled", 0,
       kMaximumReportSize10K, kReportBucketCount));

  switch (throttling_history_) {
    case ThrottlingHistory::kInitial:
    case ThrottlingHistory::kNotThrottled:
      if (context_->IsMainFrame())
        main_frame_not_throttled.Count(maximum_running_requests_seen_);
      else
        sub_frame_not_throttled.Count(maximum_running_requests_seen_);
      break;
    case ThrottlingHistory::kThrottled:
      if (context_->IsMainFrame())
        main_frame_throttled.Count(maximum_running_requests_seen_);
      else
        sub_frame_throttled.Count(maximum_running_requests_seen_);
      break;
    case ThrottlingHistory::kPartiallyThrottled:
      if (context_->IsMainFrame())
        main_frame_partially_throttled.Count(maximum_running_requests_seen_);
      else
        sub_frame_partially_throttled.Count(maximum_running_requests_seen_);
      break;
    case ThrottlingHistory::kStopped:
      break;
  }
}

bool ResourceLoadScheduler::IsThrottablePriority(
    ResourceLoadPriority priority) const {
  if (!Platform::Current()->IsRendererSideResourceSchedulerEnabled())
    return true;

  if (RuntimeEnabledFeatures::ResourceLoadSchedulerEnabled()) {
    // If this scheduler is throttled by the associated WebFrameScheduler,
    // consider every prioritiy as throttable.
    const auto state = frame_scheduler_throttling_state_;
    if (state == WebFrameScheduler::ThrottlingState::kThrottled ||
        state == WebFrameScheduler::ThrottlingState::kStopped) {
      return true;
    }
  }

  return priority < ResourceLoadPriority::kMedium;
}

void ResourceLoadScheduler::OnThrottlingStateChanged(
    WebFrameScheduler::ThrottlingState state) {
  if (traffic_monitor_)
    traffic_monitor_->OnThrottlingStateChanged(state);

  frame_scheduler_throttling_state_ = state;

  switch (state) {
    case WebFrameScheduler::ThrottlingState::kThrottled:
      if (throttling_history_ == ThrottlingHistory::kInitial)
        throttling_history_ = ThrottlingHistory::kThrottled;
      else if (throttling_history_ == ThrottlingHistory::kNotThrottled)
        throttling_history_ = ThrottlingHistory::kPartiallyThrottled;
      SetOutstandingLimitAndMaybeRun(outstanding_throttled_limit_);
      break;
    case WebFrameScheduler::ThrottlingState::kNotThrottled:
      if (throttling_history_ == ThrottlingHistory::kInitial)
        throttling_history_ = ThrottlingHistory::kNotThrottled;
      else if (throttling_history_ == ThrottlingHistory::kThrottled)
        throttling_history_ = ThrottlingHistory::kPartiallyThrottled;
      SetOutstandingLimitAndMaybeRun(kOutstandingUnlimited);
      break;
    case WebFrameScheduler::ThrottlingState::kStopped:
      throttling_history_ = ThrottlingHistory::kStopped;
      SetOutstandingLimitAndMaybeRun(0u);
      break;
  }
}

ResourceLoadScheduler::ClientId ResourceLoadScheduler::GenerateClientId() {
  ClientId id = ++current_id_;
  CHECK_NE(0u, id);
  return id;
}

void ResourceLoadScheduler::MaybeRun() {
  // Requests for keep-alive loaders could be remained in the pending queue,
  // but ignore them once Shutdown() is called.
  if (is_shutdown_)
    return;

  while (!pending_requests_.empty()) {
    if (running_requests_.size() >= outstanding_limit_)
      return;
    ClientId id = pending_requests_.begin()->client_id;
    pending_requests_.erase(pending_requests_.begin());
    auto found = pending_request_map_.find(id);
    if (found == pending_request_map_.end())
      continue;  // Already released.
    ResourceLoadSchedulerClient* client = found->value->client;
    pending_request_map_.erase(found);
    Run(id, client);
  }
}

void ResourceLoadScheduler::Run(ResourceLoadScheduler::ClientId id,
                                ResourceLoadSchedulerClient* client) {
  running_requests_.insert(id);
  if (running_requests_.size() > maximum_running_requests_seen_) {
    maximum_running_requests_seen_ = running_requests_.size();
  }
  client->Run();
}

void ResourceLoadScheduler::SetOutstandingLimitAndMaybeRun(size_t limit) {
  outstanding_limit_ = limit;
  MaybeRun();
}

}  // namespace blink
