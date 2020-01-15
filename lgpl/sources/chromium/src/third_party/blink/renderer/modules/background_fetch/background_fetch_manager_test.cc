// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/request_or_usv_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/request_or_usv_string_or_request_or_usv_string_sequence.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/request_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"

namespace blink {

class BackgroundFetchManagerTest : public testing::Test {
 protected:
  // Creates a vector of FetchAPIRequestPtr entries for the given |requests|
  // based on the |scope|. Proxied in the fixture to reduce the number of friend
  // declarations necessary in the BackgroundFetchManager.
  Vector<mojom::blink::FetchAPIRequestPtr> CreateFetchAPIRequestVector(
      V8TestingScope& scope,
      const RequestOrUSVStringOrRequestOrUSVStringSequence& requests) {
    bool has_requests_with_body;
    return BackgroundFetchManager::CreateFetchAPIRequestVector(
        scope.GetScriptState(), requests, scope.GetExceptionState(),
        &has_requests_with_body);
  }
};

TEST_F(BackgroundFetchManagerTest, NullValue) {
  V8TestingScope scope;

  RequestOrUSVStringOrRequestOrUSVStringSequence requests;

  Vector<mojom::blink::FetchAPIRequestPtr> fetch_api_requests =
      CreateFetchAPIRequestVector(scope, requests);
  ASSERT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<ESErrorType>(),
            ESErrorType::kTypeError);
}

TEST_F(BackgroundFetchManagerTest, SingleUSVString) {
  V8TestingScope scope;

  KURL image_url("https://www.example.com/my_image.png");

  RequestOrUSVStringOrRequestOrUSVStringSequence requests =
      RequestOrUSVStringOrRequestOrUSVStringSequence::FromUSVString(
          image_url.GetString());

  Vector<mojom::blink::FetchAPIRequestPtr> fetch_api_requests =
      CreateFetchAPIRequestVector(scope, requests);
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  ASSERT_EQ(fetch_api_requests.size(), 1u);
  EXPECT_EQ(fetch_api_requests[0]->url, image_url);
  EXPECT_EQ(fetch_api_requests[0]->method, "GET");
}

TEST_F(BackgroundFetchManagerTest, SingleRequest) {
  V8TestingScope scope;

  KURL image_url("https://www.example.com/my_image.png");

  RequestInit* request_init = RequestInit::Create();
  request_init->setMethod("POST");
  Request* request =
      Request::Create(scope.GetScriptState(), image_url.GetString(),
                      request_init, scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_TRUE(request);

  RequestOrUSVStringOrRequestOrUSVStringSequence requests =
      RequestOrUSVStringOrRequestOrUSVStringSequence::FromRequest(request);

  Vector<mojom::blink::FetchAPIRequestPtr> fetch_api_requests =
      CreateFetchAPIRequestVector(scope, requests);
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  ASSERT_EQ(fetch_api_requests.size(), 1u);
  EXPECT_EQ(fetch_api_requests[0]->url, image_url);
  EXPECT_EQ(fetch_api_requests[0]->method, "POST");
}

TEST_F(BackgroundFetchManagerTest, Sequence) {
  V8TestingScope scope;

  KURL image_url("https://www.example.com/my_image.png");
  KURL icon_url("https://www.example.com/my_icon.jpg");
  KURL cat_video_url("https://www.example.com/my_cat_video.avi");

  RequestOrUSVString image_request =
      RequestOrUSVString::FromUSVString(image_url.GetString());
  RequestOrUSVString icon_request =
      RequestOrUSVString::FromUSVString(icon_url.GetString());

  RequestInit* request_init = RequestInit::Create();
  request_init->setMethod("DELETE");
  Request* request =
      Request::Create(scope.GetScriptState(), cat_video_url.GetString(),
                      request_init, scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_TRUE(request);

  RequestOrUSVString cat_video_request =
      RequestOrUSVString::FromRequest(request);

  HeapVector<RequestOrUSVString> request_sequence;
  request_sequence.push_back(image_request);
  request_sequence.push_back(icon_request);
  request_sequence.push_back(cat_video_request);

  RequestOrUSVStringOrRequestOrUSVStringSequence requests =
      RequestOrUSVStringOrRequestOrUSVStringSequence::
          FromRequestOrUSVStringSequence(request_sequence);

  Vector<mojom::blink::FetchAPIRequestPtr> fetch_api_requests =
      CreateFetchAPIRequestVector(scope, requests);
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  ASSERT_EQ(fetch_api_requests.size(), 3u);
  EXPECT_EQ(fetch_api_requests[0]->url, image_url);
  EXPECT_EQ(fetch_api_requests[0]->method, "GET");

  EXPECT_EQ(fetch_api_requests[1]->url, icon_url);
  EXPECT_EQ(fetch_api_requests[1]->method, "GET");

  EXPECT_EQ(fetch_api_requests[2]->url, cat_video_url);
  EXPECT_EQ(fetch_api_requests[2]->method, "DELETE");
}

TEST_F(BackgroundFetchManagerTest, SequenceEmpty) {
  V8TestingScope scope;

  HeapVector<RequestOrUSVString> request_sequence;
  RequestOrUSVStringOrRequestOrUSVStringSequence requests =
      RequestOrUSVStringOrRequestOrUSVStringSequence::
          FromRequestOrUSVStringSequence(request_sequence);

  Vector<mojom::blink::FetchAPIRequestPtr> fetch_api_requests =
      CreateFetchAPIRequestVector(scope, requests);
  ASSERT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<ESErrorType>(),
            ESErrorType::kTypeError);
}

TEST_F(BackgroundFetchManagerTest, SequenceWithNullValue) {
  V8TestingScope scope;

  KURL image_url("https://www.example.com/my_image.png");

  RequestOrUSVString null_request;
  RequestOrUSVString image_request =
      RequestOrUSVString::FromUSVString(image_url.GetString());

  HeapVector<RequestOrUSVString> request_sequence;
  request_sequence.push_back(image_request);
  request_sequence.push_back(null_request);

  RequestOrUSVStringOrRequestOrUSVStringSequence requests =
      RequestOrUSVStringOrRequestOrUSVStringSequence::
          FromRequestOrUSVStringSequence(request_sequence);

  Vector<mojom::blink::FetchAPIRequestPtr> fetch_api_requests =
      CreateFetchAPIRequestVector(scope, requests);
  ASSERT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<ESErrorType>(),
            ESErrorType::kTypeError);
}

TEST_F(BackgroundFetchManagerTest, BlobsExtracted) {
  V8TestingScope scope;

  KURL image_url("https://www.example.com/my_image.png");
  KURL icon_url("https://www.example.com/my_icon.jpg");

  // Create first request with a body.
  String body_text = "cat_pic";
  RequestInit* request_init = RequestInit::Create();
  request_init->setMethod("POST");
  request_init->setBody(blink::ScriptValue(
      scope.GetIsolate(), ToV8(body_text, scope.GetScriptState())));
  Request* image_request =
      Request::Create(scope.GetScriptState(), image_url.GetString(),
                      request_init, scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_TRUE(image_request);
  ASSERT_TRUE(image_request->HasBody());

  // Create second request without a body.
  RequestOrUSVString icon_request =
      RequestOrUSVString::FromUSVString(icon_url.GetString());

  // Create a request sequence with both requests.
  HeapVector<RequestOrUSVString> request_sequence;
  request_sequence.push_back(RequestOrUSVString::FromRequest(image_request));
  request_sequence.push_back(icon_request);

  RequestOrUSVStringOrRequestOrUSVStringSequence requests =
      RequestOrUSVStringOrRequestOrUSVStringSequence::
          FromRequestOrUSVStringSequence(request_sequence);

  // Extract the blobs.
  Vector<mojom::blink::FetchAPIRequestPtr> fetch_api_requests =
      CreateFetchAPIRequestVector(scope, requests);
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  ASSERT_EQ(fetch_api_requests.size(), 2u);

  ASSERT_TRUE(fetch_api_requests[0]->blob);
  EXPECT_EQ(fetch_api_requests[0]->blob->size(), body_text.length());

  EXPECT_FALSE(fetch_api_requests[1]->blob);
}

}  // namespace blink
