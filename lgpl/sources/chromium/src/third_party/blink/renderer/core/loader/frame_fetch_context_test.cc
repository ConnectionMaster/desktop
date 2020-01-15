/*
 * Copyright (c) 2015, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/loader/frame_fetch_context.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/frame_resource_fetcher_properties.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_violation_reporting_policy.h"

namespace blink {

using Checkpoint = testing::StrictMock<testing::MockFunction<void(int)>>;

class FrameFetchContextMockLocalFrameClient : public EmptyLocalFrameClient {
 public:
  FrameFetchContextMockLocalFrameClient() : EmptyLocalFrameClient() {}
  MOCK_METHOD0(DidDisplayContentWithCertificateErrors, void());
  MOCK_METHOD2(DispatchDidLoadResourceFromMemoryCache,
               void(const ResourceRequest&, const ResourceResponse&));
  MOCK_METHOD1(UserAgent, String(const KURL&));
  MOCK_METHOD0(MayUseClientLoFiForImageRequests, bool());
  MOCK_CONST_METHOD0(GetPreviewsStateForFrame, WebURLRequest::PreviewsState());
};

class FixedPolicySubresourceFilter : public WebDocumentSubresourceFilter {
 public:
  FixedPolicySubresourceFilter(LoadPolicy policy,
                               int* filtered_load_counter,
                               bool is_associated_with_ad_subframe)
      : policy_(policy), filtered_load_counter_(filtered_load_counter) {}

  LoadPolicy GetLoadPolicy(const WebURL& resource_url,
                           mojom::RequestContextType) override {
    return policy_;
  }

  LoadPolicy GetLoadPolicyForWebSocketConnect(const WebURL& url) override {
    return policy_;
  }

  void ReportDisallowedLoad() override { ++*filtered_load_counter_; }

  bool ShouldLogToConsole() override { return false; }

 private:
  const LoadPolicy policy_;
  int* filtered_load_counter_;
};

class FrameFetchContextTest : public testing::Test {
 protected:
  void SetUp() override { RecreateFetchContext(); }

  void RecreateFetchContext(const KURL& url = KURL(),
                            const String& feature_policy_header = String()) {
    dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(500, 500));
    dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(1.0);
    if (url.IsValid()) {
      auto params = WebNavigationParams::CreateWithHTMLBuffer(
          SharedBuffer::Create(), url);
      if (!feature_policy_header.IsEmpty()) {
        params->response.SetHttpHeaderField(http_names::kFeaturePolicy,
                                            feature_policy_header);
      }
      dummy_page_holder->GetFrame().Loader().CommitNavigation(
          std::move(params), nullptr /* extra_data */);
      blink::test::RunPendingTasks();
      ASSERT_EQ(url.GetString(),
                dummy_page_holder->GetDocument().Url().GetString());
    }
    document = &dummy_page_holder->GetDocument();
    owner = MakeGarbageCollected<DummyFrameOwner>();
  }

  FrameFetchContext* GetFetchContext() {
    return static_cast<FrameFetchContext*>(&document->Fetcher()->Context());
  }

  // Call the method for the actual test cases as only this fixture is specified
  // as a friend class.
  void SetFirstPartyCookie(ResourceRequest& request) {
    GetFetchContext()->SetFirstPartyCookie(request);
  }

  scoped_refptr<const SecurityOrigin> GetTopFrameOrigin() {
    return GetFetchContext()->GetTopFrameOrigin();
  }

  std::unique_ptr<DummyPageHolder> dummy_page_holder;
  // We don't use the DocumentLoader directly in any tests, but need to keep it
  // around as long as the ResourceFetcher and Document live due to indirect
  // usage.
  Persistent<Document> document;

  Persistent<DummyFrameOwner> owner;
};

class FrameFetchContextSubresourceFilterTest : public FrameFetchContextTest {
 protected:
  void SetUp() override {
    FrameFetchContextTest::SetUp();
    filtered_load_callback_counter_ = 0;
  }

  void TearDown() override {
    document->Loader()->SetSubresourceFilter(nullptr);
    FrameFetchContextTest::TearDown();
  }

  int GetFilteredLoadCallCount() const {
    return filtered_load_callback_counter_;
  }

  void SetFilterPolicy(WebDocumentSubresourceFilter::LoadPolicy policy,
                       bool is_associated_with_ad_subframe = false) {
    document->Loader()->SetSubresourceFilter(SubresourceFilter::Create(
        *document, std::make_unique<FixedPolicySubresourceFilter>(
                       policy, &filtered_load_callback_counter_,
                       is_associated_with_ad_subframe)));
  }

  base::Optional<ResourceRequestBlockedReason> CanRequest() {
    return CanRequestInternal(SecurityViolationReportingPolicy::kReport);
  }

  base::Optional<ResourceRequestBlockedReason> CanRequestKeepAlive() {
    return CanRequestInternal(SecurityViolationReportingPolicy::kReport,
                              true /* keepalive */);
  }

  base::Optional<ResourceRequestBlockedReason> CanRequestPreload() {
    return CanRequestInternal(
        SecurityViolationReportingPolicy::kSuppressReporting);
  }

  base::Optional<ResourceRequestBlockedReason> CanRequestAndVerifyIsAd(
      bool expect_is_ad) {
    base::Optional<ResourceRequestBlockedReason> reason =
        CanRequestInternal(SecurityViolationReportingPolicy::kReport);
    ResourceRequest request(KURL("http://example.com/"));
    EXPECT_EQ(expect_is_ad, GetFetchContext()->CalculateIfAdSubresource(
                                request, ResourceType::kMock));
    return reason;
  }

  void AppendExecutingScriptToAdTracker(const String& url) {
    AdTracker* ad_tracker = document->GetFrame()->GetAdTracker();
    ad_tracker->WillExecuteScript(document, url);
  }

  void AppendAdScriptToAdTracker(const KURL& ad_script_url) {
    AdTracker* ad_tracker = document->GetFrame()->GetAdTracker();
    ad_tracker->AppendToKnownAdScripts(*(document.Get()),
                                       ad_script_url.GetString());
  }

 private:
  base::Optional<ResourceRequestBlockedReason> CanRequestInternal(
      SecurityViolationReportingPolicy reporting_policy,
      bool keepalive = false) {
    const KURL input_url("http://example.com/");
    ResourceRequest resource_request(input_url);
    resource_request.SetKeepalive(keepalive);
    resource_request.SetRequestorOrigin(document->Fetcher()
                                            ->GetProperties()
                                            .GetFetchClientSettingsObject()
                                            .GetSecurityOrigin());
    ResourceLoaderOptions options;
    return GetFetchContext()->CanRequest(
        ResourceType::kImage, resource_request, input_url, options,
        reporting_policy, ResourceRequest::RedirectStatus::kNoRedirect);
  }

  int filtered_load_callback_counter_;
};

// This test class sets up a mock frame loader client.
class FrameFetchContextMockedLocalFrameClientTest
    : public FrameFetchContextTest {
 protected:
  void SetUp() override {
    url = KURL("https://example.test/foo");
    http_url = KURL("http://example.test/foo");
    main_resource_url = KURL("https://example.test");
    different_host_url = KURL("https://different.example.test/foo");
    client = MakeGarbageCollected<
        testing::NiceMock<FrameFetchContextMockLocalFrameClient>>();
    dummy_page_holder =
        std::make_unique<DummyPageHolder>(IntSize(500, 500), nullptr, client);
    dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(1.0);
    Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
    document = &dummy_page_holder->GetDocument();
    document->SetURL(main_resource_url);
    owner = MakeGarbageCollected<DummyFrameOwner>();
  }

  KURL url;
  KURL http_url;
  KURL main_resource_url;
  KURL different_host_url;

  Persistent<testing::NiceMock<FrameFetchContextMockLocalFrameClient>> client;
};

class FrameFetchContextModifyRequestTest : public FrameFetchContextTest {
 public:
  FrameFetchContextModifyRequestTest()
      : example_origin(SecurityOrigin::Create(KURL("https://example.test/"))) {}

 protected:
  void ModifyRequestForCSP(ResourceRequest& resource_request,
                           network::mojom::RequestContextFrameType frame_type) {
    document->GetFrame()->Loader().RecordLatestRequiredCSP();
    document->GetFrame()->Loader().ModifyRequestForCSP(
        resource_request,
        &document->Fetcher()->GetProperties().GetFetchClientSettingsObject(),
        document.Get(), frame_type);
  }

  void ExpectUpgrade(const char* input, const char* expected) {
    ExpectUpgrade(input, mojom::RequestContextType::SCRIPT,
                  network::mojom::RequestContextFrameType::kNone, expected);
  }

  void ExpectUpgrade(const char* input,
                     mojom::RequestContextType request_context,
                     network::mojom::RequestContextFrameType frame_type,
                     const char* expected) {
    const KURL input_url(input);
    const KURL expected_url(expected);

    ResourceRequest resource_request(input_url);
    resource_request.SetRequestContext(request_context);

    ModifyRequestForCSP(resource_request, frame_type);

    EXPECT_EQ(expected_url.GetString(), resource_request.Url().GetString());
    EXPECT_EQ(expected_url.Protocol(), resource_request.Url().Protocol());
    EXPECT_EQ(expected_url.Host(), resource_request.Url().Host());
    EXPECT_EQ(expected_url.Port(), resource_request.Url().Port());
    EXPECT_EQ(expected_url.HasPort(), resource_request.Url().HasPort());
    EXPECT_EQ(expected_url.GetPath(), resource_request.Url().GetPath());
  }

  void ExpectUpgradeInsecureRequestHeader(
      const char* input,
      network::mojom::RequestContextFrameType frame_type,
      bool should_prefer) {
    const KURL input_url(input);

    ResourceRequest resource_request(input_url);
    resource_request.SetRequestContext(mojom::RequestContextType::SCRIPT);

    ModifyRequestForCSP(resource_request, frame_type);

    EXPECT_EQ(
        should_prefer ? String("1") : String(),
        resource_request.HttpHeaderField(http_names::kUpgradeInsecureRequests));

    // Calling modifyRequestForCSP more than once shouldn't affect the
    // header.
    if (should_prefer) {
      GetFetchContext()->ModifyRequestForCSP(resource_request);
      EXPECT_EQ("1", resource_request.HttpHeaderField(
                         http_names::kUpgradeInsecureRequests));
    }
  }

  void ExpectIsAutomaticUpgradeSet(const char* input,
                                   const char* main_frame,
                                   WebInsecureRequestPolicy policy,
                                   bool expected_value) {
    const KURL input_url(input);
    const KURL main_frame_url(main_frame);
    ResourceRequest resource_request(input_url);
    resource_request.SetRequestContext(mojom::RequestContextType::SCRIPT);

    RecreateFetchContext(main_frame_url);
    document->SetInsecureRequestPolicy(policy);

    ModifyRequestForCSP(resource_request,
                        network::mojom::RequestContextFrameType::kNone);

    EXPECT_EQ(expected_value, resource_request.IsAutomaticUpgrade());
  }

  void ExpectSetRequiredCSPRequestHeader(
      const char* input,
      network::mojom::RequestContextFrameType frame_type,
      const AtomicString& expected_required_csp) {
    const KURL input_url(input);
    ResourceRequest resource_request(input_url);
    resource_request.SetRequestContext(mojom::RequestContextType::SCRIPT);

    ModifyRequestForCSP(resource_request, frame_type);

    EXPECT_EQ(expected_required_csp,
              resource_request.HttpHeaderField(http_names::kSecRequiredCSP));
  }

  void SetFrameOwnerBasedOnFrameType(
      network::mojom::RequestContextFrameType frame_type,
      HTMLIFrameElement* iframe,
      const AtomicString& potential_value) {
    if (frame_type != network::mojom::RequestContextFrameType::kNested) {
      document->GetFrame()->SetOwner(nullptr);
      return;
    }

    iframe->setAttribute(html_names::kCspAttr, potential_value);
    document->GetFrame()->SetOwner(iframe);
  }

  scoped_refptr<const SecurityOrigin> example_origin;
};

TEST_F(FrameFetchContextModifyRequestTest, UpgradeInsecureResourceRequests) {
  struct TestCase {
    const char* original;
    const char* upgraded;
  } tests[] = {
      {"http://example.test/image.png", "https://example.test/image.png"},
      {"http://example.test:80/image.png",
       "https://example.test:443/image.png"},
      {"http://example.test:1212/image.png",
       "https://example.test:1212/image.png"},

      {"https://example.test/image.png", "https://example.test/image.png"},
      {"https://example.test:80/image.png",
       "https://example.test:80/image.png"},
      {"https://example.test:1212/image.png",
       "https://example.test:1212/image.png"},

      {"ftp://example.test/image.png", "ftp://example.test/image.png"},
      {"ftp://example.test:21/image.png", "ftp://example.test:21/image.png"},
      {"ftp://example.test:1212/image.png",
       "ftp://example.test:1212/image.png"},
  };

  document->SetInsecureRequestPolicy(kUpgradeInsecureRequests);

  for (const auto& test : tests) {
    document->ClearInsecureNavigationsToUpgradeForTest();

    // We always upgrade for FrameTypeNone.
    ExpectUpgrade(test.original, mojom::RequestContextType::SCRIPT,
                  network::mojom::RequestContextFrameType::kNone,
                  test.upgraded);

    // We never upgrade for FrameTypeNested. This is done on the browser
    // process.
    ExpectUpgrade(test.original, mojom::RequestContextType::SCRIPT,
                  network::mojom::RequestContextFrameType::kNested,
                  test.original);

    // We do not upgrade for FrameTypeTopLevel or FrameTypeAuxiliary...
    ExpectUpgrade(test.original, mojom::RequestContextType::SCRIPT,
                  network::mojom::RequestContextFrameType::kTopLevel,
                  test.original);
    ExpectUpgrade(test.original, mojom::RequestContextType::SCRIPT,
                  network::mojom::RequestContextFrameType::kAuxiliary,
                  test.original);

    // unless the request context is RequestContextForm.
    ExpectUpgrade(test.original, mojom::RequestContextType::FORM,
                  network::mojom::RequestContextFrameType::kTopLevel,
                  test.upgraded);
    ExpectUpgrade(test.original, mojom::RequestContextType::FORM,
                  network::mojom::RequestContextFrameType::kAuxiliary,
                  test.upgraded);

    // Or unless the host of the resource is in the document's
    // InsecureNavigationsSet:
    document->AddInsecureNavigationUpgrade(
        example_origin->Host().Impl()->GetHash());
    ExpectUpgrade(test.original, mojom::RequestContextType::SCRIPT,
                  network::mojom::RequestContextFrameType::kTopLevel,
                  test.upgraded);
    ExpectUpgrade(test.original, mojom::RequestContextType::SCRIPT,
                  network::mojom::RequestContextFrameType::kAuxiliary,
                  test.upgraded);
  }
}

TEST_F(FrameFetchContextModifyRequestTest,
       DoNotUpgradeInsecureResourceRequests) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(blink::features::kMixedContentAutoupgrade);

  RecreateFetchContext(KURL("https://secureorigin.test/image.png"));
  document->SetInsecureRequestPolicy(kLeaveInsecureRequestsAlone);

  ExpectUpgrade("http://example.test/image.png",
                "http://example.test/image.png");
  ExpectUpgrade("http://example.test:80/image.png",
                "http://example.test:80/image.png");
  ExpectUpgrade("http://example.test:1212/image.png",
                "http://example.test:1212/image.png");

  ExpectUpgrade("https://example.test/image.png",
                "https://example.test/image.png");
  ExpectUpgrade("https://example.test:80/image.png",
                "https://example.test:80/image.png");
  ExpectUpgrade("https://example.test:1212/image.png",
                "https://example.test:1212/image.png");

  ExpectUpgrade("ftp://example.test/image.png", "ftp://example.test/image.png");
  ExpectUpgrade("ftp://example.test:21/image.png",
                "ftp://example.test:21/image.png");
  ExpectUpgrade("ftp://example.test:1212/image.png",
                "ftp://example.test:1212/image.png");
}

TEST_F(FrameFetchContextModifyRequestTest, IsAutomaticUpgradeSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kMixedContentAutoupgrade);
  ExpectIsAutomaticUpgradeSet("http://example.test/image.png",
                              "https://example.test",
                              kLeaveInsecureRequestsAlone, true);
}

TEST_F(FrameFetchContextModifyRequestTest, IsAutomaticUpgradeNotSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kMixedContentAutoupgrade);
  // Upgrade shouldn't happen if the resource is already https.
  ExpectIsAutomaticUpgradeSet("https://example.test/image.png",
                              "https://example.test",
                              kLeaveInsecureRequestsAlone, false);
  // Upgrade shouldn't happen if the site is http.
  ExpectIsAutomaticUpgradeSet("http://example.test/image.png",
                              "http://example.test",
                              kLeaveInsecureRequestsAlone, false);

  // Flag shouldn't be set if upgrade was due to upgrade-insecure-requests.
  ExpectIsAutomaticUpgradeSet("http://example.test/image.png",
                              "https://example.test", kUpgradeInsecureRequests,
                              false);
}

TEST_F(FrameFetchContextModifyRequestTest, SendUpgradeInsecureRequestHeader) {
  struct TestCase {
    const char* to_request;
    network::mojom::RequestContextFrameType frame_type;
    bool should_prefer;
  } tests[] = {{"http://example.test/page.html",
                network::mojom::RequestContextFrameType::kAuxiliary, true},
               {"http://example.test/page.html",
                network::mojom::RequestContextFrameType::kNested, true},
               {"http://example.test/page.html",
                network::mojom::RequestContextFrameType::kNone, false},
               {"http://example.test/page.html",
                network::mojom::RequestContextFrameType::kTopLevel, true},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kAuxiliary, true},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kNested, true},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kNone, false},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kTopLevel, true}};

  // This should work correctly both when the FrameFetchContext has a Document,
  // and when it doesn't (e.g. during main frame navigations), so run through
  // the tests both before and after providing a document to the context.
  for (const auto& test : tests) {
    document->SetInsecureRequestPolicy(kLeaveInsecureRequestsAlone);
    ExpectUpgradeInsecureRequestHeader(test.to_request, test.frame_type,
                                       test.should_prefer);

    document->SetInsecureRequestPolicy(kUpgradeInsecureRequests);
    ExpectUpgradeInsecureRequestHeader(test.to_request, test.frame_type,
                                       test.should_prefer);
  }

  for (const auto& test : tests) {
    document->SetInsecureRequestPolicy(kLeaveInsecureRequestsAlone);
    ExpectUpgradeInsecureRequestHeader(test.to_request, test.frame_type,
                                       test.should_prefer);

    document->SetInsecureRequestPolicy(kUpgradeInsecureRequests);
    ExpectUpgradeInsecureRequestHeader(test.to_request, test.frame_type,
                                       test.should_prefer);
  }
}

TEST_F(FrameFetchContextModifyRequestTest, SendRequiredCSPHeader) {
  struct TestCase {
    const char* to_request;
    network::mojom::RequestContextFrameType frame_type;
  } tests[] = {{"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kAuxiliary},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kNested},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kNone},
               {"https://example.test/page.html",
                network::mojom::RequestContextFrameType::kTopLevel}};

  auto* iframe = MakeGarbageCollected<HTMLIFrameElement>(*document);
  const AtomicString& required_csp = AtomicString("default-src 'none'");
  const AtomicString& another_required_csp = AtomicString("default-src 'self'");

  for (const auto& test : tests) {
    SetFrameOwnerBasedOnFrameType(test.frame_type, iframe, required_csp);
    ExpectSetRequiredCSPRequestHeader(
        test.to_request, test.frame_type,
        test.frame_type == network::mojom::RequestContextFrameType::kNested
            ? required_csp
            : g_null_atom);

    SetFrameOwnerBasedOnFrameType(test.frame_type, iframe,
                                  another_required_csp);
    ExpectSetRequiredCSPRequestHeader(
        test.to_request, test.frame_type,
        test.frame_type == network::mojom::RequestContextFrameType::kNested
            ? another_required_csp
            : g_null_atom);
  }
}

class FrameFetchContextHintsTest : public FrameFetchContextTest {
 public:
  FrameFetchContextHintsTest() = default;

  void SetUp() override {
    // Set the document URL to a secure document.
    RecreateFetchContext(KURL("https://www.example.com/"));
    Settings* settings = document->GetSettings();
    settings->SetScriptEnabled(true);
  }

 protected:
  void ExpectHeader(const char* input,
                    const char* header_name,
                    bool is_present,
                    const char* header_value,
                    float width = 0) {
    SCOPED_TRACE(testing::Message() << header_name);
    ClientHintsPreferences hints_preferences;

    FetchParameters::ResourceWidth resource_width;
    if (width > 0) {
      resource_width.width = width;
      resource_width.is_set = true;
    }

    const KURL input_url(input);
    ResourceRequest resource_request(input_url);

    GetFetchContext()->AddClientHintsIfNecessary(
        hints_preferences, resource_width, resource_request);

    EXPECT_EQ(is_present ? String(header_value) : String(),
              resource_request.HttpHeaderField(header_name));
  }

  String GetHeaderValue(const char* input, const char* header_name) {
    ClientHintsPreferences hints_preferences;
    FetchParameters::ResourceWidth resource_width;
    const KURL input_url(input);
    ResourceRequest resource_request(input_url);
    GetFetchContext()->AddClientHintsIfNecessary(
        hints_preferences, resource_width, resource_request);
    return resource_request.HttpHeaderField(header_name);
  }
};

// Verify that the client hints should be attached for subresources fetched
// over secure transport. Tests when the persistent client hint feature is
// enabled.
TEST_F(FrameFetchContextHintsTest, MonitorDeviceMemorySecureTransport) {
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "4");
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
  // Without a feature policy header, the client hints should be sent only to
  // the first party origins.
  ExpectHeader("https://www.someother-example.com/1.gif", "Device-Memory",
               false, "");
}

// Verify that client hints are not attached when the resources do not belong to
// a secure context.
TEST_F(FrameFetchContextHintsTest, MonitorDeviceMemoryHintsInsecureContext) {
  // Verify that client hints are not attached when the resources do not belong
  // to a secure context and the persistent client hint features is enabled.
  ExpectHeader("http://www.example.com/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("http://www.example.com/1.gif", "Device-Memory", false, "");
  ExpectHeader("http://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
}

// Verify that client hints are attched when the resources belong to a local
// context.
TEST_F(FrameFetchContextHintsTest, MonitorDeviceMemoryHintsLocalContext) {
  RecreateFetchContext(KURL("http://localhost/"));
  document->GetSettings()->SetScriptEnabled(true);
  ExpectHeader("http://localhost/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("http://localhost/1.gif", "Device-Memory", true, "4");
  ExpectHeader("http://localhost/1.gif", "DPR", false, "");
  ExpectHeader("http://localhost/1.gif", "Width", false, "");
  ExpectHeader("http://localhost/1.gif", "Viewport-Width", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorDeviceMemoryHints) {
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "4");
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(2048);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "2");
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(64385);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "8");
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(768);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "0.5");
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorDPRHints) {
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDpr);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("https://www.example.com/1.gif", "DPR", true, "1");
  dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(2.5);
  ExpectHeader("https://www.example.com/1.gif", "DPR", true, "2.5");
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorDPRHintsInsecureTransport) {
    ExpectHeader("http://www.example.com/1.gif", "DPR", false, "");
    dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(2.5);
    ExpectHeader("http://www.example.com/1.gif", "DPR", false, "  ");
    ExpectHeader("http://www.example.com/1.gif", "Width", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorResourceWidthHints) {
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kResourceWidth);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "500", 500);
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "667", 666.6666);
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  dummy_page_holder->GetPage().SetDeviceScaleFactorDeprecated(2.5);
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "1250", 500);
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "1667",
               666.6666);
}

TEST_F(FrameFetchContextHintsTest, MonitorViewportWidthHints) {
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kViewportWidth);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", true, "500");
  dummy_page_holder->GetFrameView().SetLayoutSizeFixedToFrameSize(false);
  dummy_page_holder->GetFrameView().SetLayoutSize(IntSize(800, 800));
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", true, "800");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", true, "800",
               666.6666);
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorLangHint) {
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Lang", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Lang", false, "");

  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kLang);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

  document->domWindow()->navigator()->SetLanguagesForTesting("en-US");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Lang", true,
               "\"en-US\"");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Lang", false, "");

  document->domWindow()->navigator()->SetLanguagesForTesting("en,de,fr");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Lang", true,
               "\"en\", \"de\", \"fr\"");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Lang", false, "");

  document->domWindow()->navigator()->SetLanguagesForTesting(
      "en-US,fr_FR,de-DE,es");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Lang", true,
               "\"en-US\", \"fr-FR\", \"de-DE\", \"es\"");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Lang", false, "");
}

TEST_F(FrameFetchContextHintsTest, MonitorUAHints) {
  // `Sec-CH-UA` is always sent for secure requests
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA", true, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA", false, "");

  // `Sec-CH-UA-*` requires opt-in.
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform", false,
               "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Platform", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");

  {
    ClientHintsPreferences preferences;
    preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kUAArch);
    document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", true, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform", false,
                 "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");

    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Platform", false,
                 "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
  }

  {
    ClientHintsPreferences preferences;
    preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kUAPlatform);
    document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform", true,
                 "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");

    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Platform", false,
                 "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
  }

  {
    ClientHintsPreferences preferences;
    preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kUAModel);
    document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform", false,
                 "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", true, "");

    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Platform", false,
                 "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
  }
}

TEST_F(FrameFetchContextHintsTest, MonitorAllHints) {
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", false, "");
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "rtt", false, "");
  ExpectHeader("https://www.example.com/1.gif", "downlink", false, "");
  ExpectHeader("https://www.example.com/1.gif", "ect", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Lang", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform", false,
               "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");

  // `Sec-CH-UA` is special.
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA", true, "");

  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDpr);
  preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kResourceWidth);
  preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kViewportWidth);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kRtt);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDownlink);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kEct);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kLang);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kUA);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kUAArch);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kUAPlatform);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kUAModel);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "4");
  ExpectHeader("https://www.example.com/1.gif", "DPR", true, "1");
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "400", 400);
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", true, "500");

  document->domWindow()->navigator()->SetLanguagesForTesting("en,de,fr");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Lang", true,
               "\"en\", \"de\", \"fr\"");

  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA", true, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", true, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform", true, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", true, "");

  // Value of network quality client hints may vary, so only check if the
  // header is present and the values are non-negative/non-empty.
  bool conversion_ok = false;
  int rtt_header_value = GetHeaderValue("https://www.example.com/1.gif", "rtt")
                             .ToIntStrict(&conversion_ok);
  EXPECT_TRUE(conversion_ok);
  EXPECT_LE(0, rtt_header_value);

  float downlink_header_value =
      GetHeaderValue("https://www.example.com/1.gif", "downlink")
          .ToFloat(&conversion_ok);
  EXPECT_TRUE(conversion_ok);
  EXPECT_LE(0, downlink_header_value);

  EXPECT_LT(
      0u,
      GetHeaderValue("https://www.example.com/1.gif", "ect").Ascii().length());
}

// Verify that the client hints should be attached for third-party subresources
// fetched over secure transport, when specifically allowed by feature policy.
TEST_F(FrameFetchContextHintsTest, MonitorAllHintsFeaturePolicy) {
  RecreateFetchContext(
      KURL("https://www.example.com/"),
      "ch-dpr *; ch-device-memory *; ch-downlink *; ch-ect *; ch-lang *;"
      "ch-rtt *; ch-ua *; ch-ua-arch *; ch-ua-platform *; ch-ua-model *;"
      "ch-viewport-width *; ch-width *");
  document->GetSettings()->SetScriptEnabled(true);
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDpr);
  preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kResourceWidth);
  preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kViewportWidth);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kRtt);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDownlink);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kEct);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kLang);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kUA);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kUAArch);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kUAPlatform);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kUAModel);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

  // Verify that all client hints are sent to a third-party origin, with this
  // feature policy header.
  ExpectHeader("https://www.example.net/1.gif", "DPR", true, "1");
  ExpectHeader("https://www.example.net/1.gif", "Device-Memory", true, "4");

  document->domWindow()->navigator()->SetLanguagesForTesting("en,de,fr");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Lang", true,
               "\"en\", \"de\", \"fr\"");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA", true, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Arch", true, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Platform", true, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Model", true, "");
  ExpectHeader("https://www.example.net/1.gif", "Width", true, "400", 400);
  ExpectHeader("https://www.example.net/1.gif", "Viewport-Width", true, "500");

  // Value of network quality client hints may vary, so only check if the
  // header is present and the values are non-negative/non-empty.
  bool conversion_ok = false;
  int rtt_header_value = GetHeaderValue("https://www.example.com/1.gif", "rtt")
                             .ToIntStrict(&conversion_ok);
  EXPECT_TRUE(conversion_ok);
  EXPECT_LE(0, rtt_header_value);

  float downlink_header_value =
      GetHeaderValue("https://www.example.com/1.gif", "downlink")
          .ToFloat(&conversion_ok);
  EXPECT_TRUE(conversion_ok);
  EXPECT_LE(0, downlink_header_value);

  EXPECT_LT(
      0u,
      GetHeaderValue("https://www.example.com/1.gif", "ect").Ascii().length());
}

// Verify that only the specifically allowed client hints are attached for
// third-party subresources fetched over secure transport.
TEST_F(FrameFetchContextHintsTest, MonitorSomeHintsFeaturePolicy) {
  RecreateFetchContext(KURL("https://www.example.com/"),
                       "ch-device-memory 'self' https://www.example.net");
  document->GetSettings()->SetScriptEnabled(true);
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDpr);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  // With a feature policy header, the client hints should be sent to the
  // declared third party origins.
  ExpectHeader("https://www.example.net/1.gif", "Device-Memory", true, "4");
  ExpectHeader("https://www.someother-example.com/1.gif", "Device-Memory",
               false, "");
  // `Sec-CH-UA` is special.
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA", true, "");

  // Other hints not declared in the policy are still not attached.
  ExpectHeader("https://www.example.net/1.gif", "downlink", false, "");
  ExpectHeader("https://www.example.net/1.gif", "ect", false, "");
  ExpectHeader("https://www.example.net/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Lang", false, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Arch", false, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Platform", false,
               "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Model", false, "");
  ExpectHeader("https://www.example.net/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.net/1.gif", "Viewport-Width", false, "");
}

// Verify that the client hints are not attached for third-party subresources
// fetched over insecure transport, even when specifically allowed by feature
// policy.
TEST_F(FrameFetchContextHintsTest, MonitorHintsFeaturePolicyInsecureContext) {
  RecreateFetchContext(KURL("https://www.example.com/"), "ch-device-memory *");
  document->GetSettings()->SetScriptEnabled(true);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSendForTesting(mojom::WebClientHintsType::kDeviceMemory);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  // Device-Memory hint in this case is sent to all (and only) secure origins.
  ExpectHeader("https://www.example.net/1.gif", "Device-Memory", true, "4");
  ExpectHeader("http://www.example.net/1.gif", "Device-Memory", false, "");
}

TEST_F(FrameFetchContextTest, SubResourceCachePolicy) {
  // Reset load event state: if the load event is finished, we ignore the
  // DocumentLoader load type.
  document->open();
  ASSERT_FALSE(document->LoadEventFinished());

  // Default case
  ResourceRequest request("http://www.example.com/mock");
  EXPECT_EQ(mojom::FetchCacheMode::kDefault,
            GetFetchContext()->ResourceRequestCachePolicy(
                request, ResourceType::kMock, FetchParameters::kNoDefer));

  // WebFrameLoadType::kReload should not affect sub-resources
  document->Loader()->SetLoadType(WebFrameLoadType::kReload);
  EXPECT_EQ(mojom::FetchCacheMode::kDefault,
            GetFetchContext()->ResourceRequestCachePolicy(
                request, ResourceType::kMock, FetchParameters::kNoDefer));

  // Conditional request
  document->Loader()->SetLoadType(WebFrameLoadType::kStandard);
  ResourceRequest conditional("http://www.example.com/mock");
  conditional.SetHttpHeaderField(http_names::kIfModifiedSince, "foo");
  EXPECT_EQ(mojom::FetchCacheMode::kValidateCache,
            GetFetchContext()->ResourceRequestCachePolicy(
                conditional, ResourceType::kMock, FetchParameters::kNoDefer));

  // WebFrameLoadType::kReloadBypassingCache
  document->Loader()->SetLoadType(WebFrameLoadType::kReloadBypassingCache);
  EXPECT_EQ(mojom::FetchCacheMode::kBypassCache,
            GetFetchContext()->ResourceRequestCachePolicy(
                request, ResourceType::kMock, FetchParameters::kNoDefer));

  // WebFrameLoadType::kReloadBypassingCache with a conditional request
  document->Loader()->SetLoadType(WebFrameLoadType::kReloadBypassingCache);
  EXPECT_EQ(mojom::FetchCacheMode::kBypassCache,
            GetFetchContext()->ResourceRequestCachePolicy(
                conditional, ResourceType::kMock, FetchParameters::kNoDefer));

  // Back/forward navigation
  document->Loader()->SetLoadType(WebFrameLoadType::kBackForward);
  EXPECT_EQ(mojom::FetchCacheMode::kForceCache,
            GetFetchContext()->ResourceRequestCachePolicy(
                request, ResourceType::kMock, FetchParameters::kNoDefer));

  // Back/forward navigation with a conditional request
  document->Loader()->SetLoadType(WebFrameLoadType::kBackForward);
  EXPECT_EQ(mojom::FetchCacheMode::kForceCache,
            GetFetchContext()->ResourceRequestCachePolicy(
                conditional, ResourceType::kMock, FetchParameters::kNoDefer));
}

// Tests if "Save-Data" header is correctly added on the first load and reload.
TEST_F(FrameFetchContextTest, EnableDataSaver) {
  GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);
  // Recreate the fetch context so that the updated save data settings are read.
  RecreateFetchContext();

  ResourceRequest resource_request("http://www.example.com");
  GetFetchContext()->AddAdditionalRequestHeaders(resource_request);
  EXPECT_EQ("on", resource_request.HttpHeaderField("Save-Data"));

  // Subsequent call to addAdditionalRequestHeaders should not append to the
  // save-data header.
  GetFetchContext()->AddAdditionalRequestHeaders(resource_request);
  EXPECT_EQ("on", resource_request.HttpHeaderField("Save-Data"));
}

// Tests if "Save-Data" header is not added when the data saver is disabled.
TEST_F(FrameFetchContextTest, DisabledDataSaver) {
  GetNetworkStateNotifier().SetSaveDataEnabledOverride(false);
  // Recreate the fetch context so that the updated save data settings are read.
  RecreateFetchContext();

  ResourceRequest resource_request("http://www.example.com");
  GetFetchContext()->AddAdditionalRequestHeaders(resource_request);
  EXPECT_EQ(String(), resource_request.HttpHeaderField("Save-Data"));
}

// Tests if reload variants can reflect the current data saver setting.
TEST_F(FrameFetchContextTest, ChangeDataSaverConfig) {
  GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);
  // Recreate the fetch context so that the updated save data settings are read.
  RecreateFetchContext();
  ResourceRequest resource_request("http://www.example.com");
  GetFetchContext()->AddAdditionalRequestHeaders(resource_request);
  EXPECT_EQ("on", resource_request.HttpHeaderField("Save-Data"));

  GetNetworkStateNotifier().SetSaveDataEnabledOverride(false);
  RecreateFetchContext();
  document->Loader()->SetLoadType(WebFrameLoadType::kReload);
  GetFetchContext()->AddAdditionalRequestHeaders(resource_request);
  EXPECT_EQ(String(), resource_request.HttpHeaderField("Save-Data"));

  GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);
  RecreateFetchContext();
  GetFetchContext()->AddAdditionalRequestHeaders(resource_request);
  EXPECT_EQ("on", resource_request.HttpHeaderField("Save-Data"));

  GetNetworkStateNotifier().SetSaveDataEnabledOverride(false);
  RecreateFetchContext();
  document->Loader()->SetLoadType(WebFrameLoadType::kReload);
  GetFetchContext()->AddAdditionalRequestHeaders(resource_request);
  EXPECT_EQ(String(), resource_request.HttpHeaderField("Save-Data"));
}

TEST_F(FrameFetchContextSubresourceFilterTest, Filter) {
  SetFilterPolicy(WebDocumentSubresourceFilter::kDisallow);

  EXPECT_EQ(ResourceRequestBlockedReason::kSubresourceFilter,
            CanRequestAndVerifyIsAd(true));
  EXPECT_EQ(1, GetFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::kSubresourceFilter,
            CanRequestAndVerifyIsAd(true));
  EXPECT_EQ(2, GetFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::kSubresourceFilter,
            CanRequestPreload());
  EXPECT_EQ(2, GetFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::kSubresourceFilter,
            CanRequestAndVerifyIsAd(true));
  EXPECT_EQ(3, GetFilteredLoadCallCount());
}

TEST_F(FrameFetchContextSubresourceFilterTest, Allow) {
  SetFilterPolicy(WebDocumentSubresourceFilter::kAllow);

  EXPECT_EQ(base::nullopt, CanRequestAndVerifyIsAd(false));
  EXPECT_EQ(0, GetFilteredLoadCallCount());

  EXPECT_EQ(base::nullopt, CanRequestPreload());
  EXPECT_EQ(0, GetFilteredLoadCallCount());
}

TEST_F(FrameFetchContextSubresourceFilterTest, DuringOnFreeze) {
  document->SetFreezingInProgress(true);
  // Only keepalive requests should succeed during onfreeze.
  EXPECT_EQ(ResourceRequestBlockedReason::kOther, CanRequest());
  EXPECT_EQ(base::nullopt, CanRequestKeepAlive());
  document->SetFreezingInProgress(false);
  EXPECT_EQ(base::nullopt, CanRequest());
  EXPECT_EQ(base::nullopt, CanRequestKeepAlive());
}

TEST_F(FrameFetchContextSubresourceFilterTest, WouldDisallow) {
  SetFilterPolicy(WebDocumentSubresourceFilter::kWouldDisallow);

  EXPECT_EQ(base::nullopt, CanRequestAndVerifyIsAd(true));
  EXPECT_EQ(0, GetFilteredLoadCallCount());

  EXPECT_EQ(base::nullopt, CanRequestPreload());
  EXPECT_EQ(0, GetFilteredLoadCallCount());
}

TEST_F(FrameFetchContextTest, AddAdditionalRequestHeadersWhenDetached) {
  const KURL document_url("https://www2.example.com/fuga/hoge.html");
  const String origin = "https://www2.example.com";
  ResourceRequest request(KURL("https://localhost/"));
  request.SetHttpMethod("PUT");

  GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);

  dummy_page_holder = nullptr;

  GetFetchContext()->AddAdditionalRequestHeaders(request);

  EXPECT_EQ(String(), request.HttpHeaderField("Save-Data"));
}

TEST_F(FrameFetchContextTest, ResourceRequestCachePolicyWhenDetached) {
  ResourceRequest request(KURL("https://localhost/"));

  dummy_page_holder = nullptr;

  EXPECT_EQ(mojom::FetchCacheMode::kDefault,
            GetFetchContext()->ResourceRequestCachePolicy(
                request, ResourceType::kRaw, FetchParameters::kNoDefer));
}

TEST_F(FrameFetchContextMockedLocalFrameClientTest,
       PrepareRequestWhenDetached) {
  Checkpoint checkpoint;

  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, UserAgent(::testing::_)).WillOnce(testing::Return(String("hi")));
  EXPECT_CALL(checkpoint, Call(2));

  checkpoint.Call(1);
  dummy_page_holder = nullptr;
  checkpoint.Call(2);

  ResourceRequest request(KURL("https://localhost/"));
  WebScopedVirtualTimePauser virtual_time_pauser;
  GetFetchContext()->PrepareRequest(request, FetchInitiatorInfo(),
                                    virtual_time_pauser, ResourceType::kRaw);

  EXPECT_EQ("hi", request.HttpHeaderField(http_names::kUserAgent));
}

TEST_F(FrameFetchContextTest, AddResourceTimingWhenDetached) {
  scoped_refptr<ResourceTimingInfo> info = ResourceTimingInfo::Create(
      "type", base::TimeTicks() + base::TimeDelta::FromSecondsD(0.3));

  dummy_page_holder = nullptr;

  GetFetchContext()->AddResourceTiming(*info);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, AllowImageWhenDetached) {
  const KURL url("https://www.example.com/");

  dummy_page_holder = nullptr;

  EXPECT_TRUE(GetFetchContext()->AllowImage(true, url));
  EXPECT_TRUE(GetFetchContext()->AllowImage(false, url));
}

TEST_F(FrameFetchContextTest, PopulateResourceRequestWhenDetached) {
  const KURL url("https://www.example.com/");
  ResourceRequest request(url);

  ClientHintsPreferences client_hints_preferences;
  client_hints_preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kDeviceMemory);
  client_hints_preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kDpr);
  client_hints_preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kResourceWidth);
  client_hints_preferences.SetShouldSendForTesting(
      mojom::WebClientHintsType::kViewportWidth);

  FetchParameters::ResourceWidth resource_width;
  ResourceLoaderOptions options;

  document->GetFrame()->GetClientHintsPreferences().SetShouldSendForTesting(
      mojom::WebClientHintsType::kDeviceMemory);
  document->GetFrame()->GetClientHintsPreferences().SetShouldSendForTesting(
      mojom::WebClientHintsType::kDpr);
  document->GetFrame()->GetClientHintsPreferences().SetShouldSendForTesting(
      mojom::WebClientHintsType::kResourceWidth);
  document->GetFrame()->GetClientHintsPreferences().SetShouldSendForTesting(
      mojom::WebClientHintsType::kViewportWidth);

  dummy_page_holder = nullptr;

  GetFetchContext()->PopulateResourceRequest(
      ResourceType::kRaw, client_hints_preferences, resource_width, request);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, SetFirstPartyCookieWhenDetached) {
  const KURL document_url("https://www2.example.com/foo/bar");
  RecreateFetchContext(document_url);

  const KURL url("https://www.example.com/hoge/fuga");
  ResourceRequest request(url);

  dummy_page_holder = nullptr;

  SetFirstPartyCookie(request);

  EXPECT_TRUE(SecurityOrigin::AreSameSchemeHostPort(document_url,
                                                    request.SiteForCookies()));
}

TEST_F(FrameFetchContextTest, TopFrameOrigin) {
  const KURL document_url("https://www2.example.com/foo/bar");
  RecreateFetchContext(document_url);
  const SecurityOrigin* origin = document->GetSecurityOrigin();

  const KURL url("https://www.example.com/hoge/fuga");
  ResourceRequest request(url);

  EXPECT_EQ(origin, GetTopFrameOrigin());
}

TEST_F(FrameFetchContextTest, TopFrameOriginDetached) {
  const KURL document_url("https://www2.example.com/foo/bar");
  RecreateFetchContext(document_url);
  const SecurityOrigin* origin = document->GetSecurityOrigin();

  const KURL url("https://www.example.com/hoge/fuga");
  ResourceRequest request(url);

  dummy_page_holder = nullptr;

  EXPECT_EQ(origin, GetTopFrameOrigin());
}

}  // namespace blink
