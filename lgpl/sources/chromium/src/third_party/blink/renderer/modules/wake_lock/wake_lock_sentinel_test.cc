// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_sentinel.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_state_record.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_test_utils.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class SyncReleaseEventListener final : public NativeEventListener {
 public:
  SyncReleaseEventListener(base::OnceClosure invocation_callback)
      : invocation_callback_(std::move(invocation_callback)) {}
  void Invoke(ExecutionContext*, Event*) override {
    DCHECK(invocation_callback_);
    std::move(invocation_callback_).Run();
  }

 private:
  base::OnceClosure invocation_callback_;
};

}  // namespace

TEST(WakeLockSentinelTest, SentinelType) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  auto* sentinel = MakeGarbageCollected<WakeLockSentinel>(
      context.GetScriptState(), WakeLockType::kScreen, /*manager=*/nullptr);
  EXPECT_EQ("screen", sentinel->type());

  sentinel = MakeGarbageCollected<WakeLockSentinel>(
      context.GetScriptState(), WakeLockType::kSystem, /*manager=*/nullptr);
  EXPECT_EQ("system", sentinel->type());
}

TEST(WakeLockSentinelTest, MultipleReleaseCalls) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  auto* state_record = MakeGarbageCollected<WakeLockStateRecord>(
      context.GetDocument(), WakeLockType::kScreen);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise = resolver->Promise();
  state_record->AcquireWakeLock(resolver);
  context.WaitForPromiseFulfillment(promise);
  auto* sentinel =
      ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(promise);
  ASSERT_NE(nullptr, sentinel);

  base::RunLoop run_loop;
  auto* event_listener =
      MakeGarbageCollected<SyncReleaseEventListener>(run_loop.QuitClosure());
  sentinel->addEventListener(event_type_names::kRelease, event_listener);
  sentinel->release(context.GetScriptState());
  run_loop.Run();
  sentinel->removeEventListener(event_type_names::kRelease, event_listener);

  EXPECT_EQ(nullptr, sentinel->manager_);

  event_listener =
      MakeGarbageCollected<SyncReleaseEventListener>(WTF::Bind([]() {
        EXPECT_TRUE(false) << "This event handler should not be reached.";
      }));
  sentinel->addEventListener(event_type_names::kRelease, event_listener);
  sentinel->release(context.GetScriptState());
}

TEST(WakeLockSentinelTest, ContextDestruction) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);

  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  auto* wake_lock = MakeGarbageCollected<WakeLock>(*context.GetDocument());
  wake_lock->DoRequest(WakeLockType::kScreen, screen_resolver);

  WakeLockStateRecord* state_record =
      wake_lock->state_records_[static_cast<size_t>(WakeLockType::kScreen)];
  ASSERT_TRUE(state_record);

  context.WaitForPromiseFulfillment(screen_promise);
  auto* sentinel = ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(
      screen_promise);
  ASSERT_TRUE(sentinel);

  auto* event_listener =
      MakeGarbageCollected<SyncReleaseEventListener>(WTF::Bind([]() {
        EXPECT_TRUE(false) << "This event handler should not be reached.";
      }));
  sentinel->addEventListener(event_type_names::kRelease, event_listener);
  EXPECT_TRUE(sentinel->HasPendingActivity());

  context.GetDocument()->Shutdown();

  // If the method returns false the object can be GC'ed.
  EXPECT_FALSE(sentinel->HasPendingActivity());
}

}  // namespace blink
