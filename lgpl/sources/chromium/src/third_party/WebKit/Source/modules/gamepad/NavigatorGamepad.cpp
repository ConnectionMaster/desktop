/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "modules/gamepad/NavigatorGamepad.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/page/Page.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "modules/gamepad/GamepadDispatcher.h"
#include "modules/gamepad/GamepadEvent.h"
#include "modules/gamepad/GamepadList.h"
#include "public/platform/TaskType.h"

namespace {

void HasGamepadConnectionChanged(const String& old_id,
                                 const String& new_id,
                                 bool old_connected,
                                 bool new_connected,
                                 bool* gamepad_found,
                                 bool* gamepad_lost) {
  // If the gamepad ID changes, treat it as a disconnection and connection.
  bool id_changed = old_connected && new_connected && old_id != new_id;

  if (gamepad_found)
    *gamepad_found = id_changed || (!old_connected && new_connected);
  if (gamepad_lost)
    *gamepad_lost = id_changed || (old_connected && !new_connected);
}

}  // namespace

namespace blink {

template <typename T>
static void SampleGamepad(unsigned index,
                          T& gamepad,
                          const device::Gamepad& device_gamepad) {
  String old_id = gamepad.id();
  bool old_was_connected = gamepad.connected();

  gamepad.SetId(device_gamepad.id);
  gamepad.SetConnected(device_gamepad.connected);
  gamepad.SetTimestamp(device_gamepad.timestamp);
  gamepad.SetAxes(device_gamepad.axes_length, device_gamepad.axes);
  gamepad.SetButtons(device_gamepad.buttons_length, device_gamepad.buttons);
  gamepad.SetPose(device_gamepad.pose);
  gamepad.SetHand(device_gamepad.hand);

  bool newly_connected;
  HasGamepadConnectionChanged(old_id, gamepad.id(), old_was_connected,
                              gamepad.connected(), &newly_connected, nullptr);

  // These fields are not expected to change and will only be written when the
  // gamepad is newly connected.
  if (newly_connected) {
    gamepad.SetIndex(index);
    gamepad.SetMapping(device_gamepad.mapping);
    gamepad.SetVibrationActuator(device_gamepad.vibration_actuator);
    gamepad.SetDisplayId(device_gamepad.display_id);
  }
}

template <typename GamepadType, typename ListType>
static void SampleGamepads(ListType* into) {
  device::Gamepads gamepads;

  GamepadDispatcher::Instance().SampleGamepads(gamepads);

  for (unsigned i = 0; i < device::Gamepads::kItemsLengthCap; ++i) {
    device::Gamepad& web_gamepad = gamepads.items[i];
    if (web_gamepad.connected) {
      GamepadType* gamepad = into->item(i);
      if (!gamepad)
        gamepad = GamepadType::Create();
      SampleGamepad(i, *gamepad, web_gamepad);
      into->Set(i, gamepad);
    } else {
      into->Set(i, nullptr);
    }
  }
}

NavigatorGamepad* NavigatorGamepad::From(Document& document) {
  if (!document.GetFrame() || !document.GetFrame()->DomWindow())
    return nullptr;
  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();
  return &From(navigator);
}

NavigatorGamepad& NavigatorGamepad::From(Navigator& navigator) {
  NavigatorGamepad* supplement = static_cast<NavigatorGamepad*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorGamepad(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

GamepadList* NavigatorGamepad::getGamepads(Navigator& navigator) {
  return NavigatorGamepad::From(navigator).Gamepads();
}

GamepadList* NavigatorGamepad::Gamepads() {
  SampleAndCheckConnectedGamepads();
  return gamepads_.Get();
}

void NavigatorGamepad::Trace(blink::Visitor* visitor) {
  visitor->Trace(gamepads_);
  visitor->Trace(gamepads_back_);
  visitor->Trace(pending_events_);
  visitor->Trace(dispatch_one_event_runner_);
  Supplement<Navigator>::Trace(visitor);
  DOMWindowClient::Trace(visitor);
  PlatformEventController::Trace(visitor);
}

bool NavigatorGamepad::StartUpdatingIfAttached() {
  // The frame must be attached to start updating.
  if (GetFrame()) {
    StartUpdating();
    return true;
  }
  return false;
}

void NavigatorGamepad::DidUpdateData() {
  // We should stop listening once we detached.
  DCHECK(GetFrame());
  DCHECK(DomWindow());

  // We register to the dispatcher before sampling gamepads so we need to check
  // if we actually have an event listener.
  if (!has_event_listener_)
    return;

  SampleAndCheckConnectedGamepads();
}

void NavigatorGamepad::DispatchOneEvent() {
  DCHECK(DomWindow());
  DCHECK(!pending_events_.IsEmpty());

  Gamepad* gamepad = pending_events_.TakeFirst();
  const AtomicString& event_name = gamepad->connected()
                                       ? EventTypeNames::gamepadconnected
                                       : EventTypeNames::gamepaddisconnected;
  DomWindow()->DispatchEvent(
      GamepadEvent::Create(event_name, false, true, gamepad));

  if (!pending_events_.IsEmpty()) {
    DCHECK(dispatch_one_event_runner_);
    dispatch_one_event_runner_->RunAsync();
  }
}

NavigatorGamepad::NavigatorGamepad(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      DOMWindowClient(navigator.DomWindow()),
      PlatformEventController(
          navigator.GetFrame() ? navigator.GetFrame()->GetDocument() : nullptr),
      dispatch_one_event_runner_(
          navigator.GetFrame() ? AsyncMethodRunner<NavigatorGamepad>::Create(
                                     this,
                                     &NavigatorGamepad::DispatchOneEvent,
                                     navigator.GetFrame()->GetTaskRunner(
                                         TaskType::kMiscPlatformAPI))
                               : nullptr) {
  if (navigator.DomWindow())
    navigator.DomWindow()->RegisterEventListenerObserver(this);
}

NavigatorGamepad::~NavigatorGamepad() {}

const char* NavigatorGamepad::SupplementName() {
  return "NavigatorGamepad";
}

void NavigatorGamepad::RegisterWithDispatcher() {
  GamepadDispatcher::Instance().AddController(this);
  if (dispatch_one_event_runner_)
    dispatch_one_event_runner_->Unpause();
}

void NavigatorGamepad::UnregisterWithDispatcher() {
  if (dispatch_one_event_runner_)
    dispatch_one_event_runner_->Pause();
  GamepadDispatcher::Instance().RemoveController(this);
}

bool NavigatorGamepad::HasLastData() {
  // Gamepad data is polled instead of pushed.
  return false;
}

static bool IsGamepadEvent(const AtomicString& event_type) {
  return event_type == EventTypeNames::gamepadconnected ||
         event_type == EventTypeNames::gamepaddisconnected;
}

void NavigatorGamepad::DidAddEventListener(LocalDOMWindow*,
                                           const AtomicString& event_type) {
  if (!IsGamepadEvent(event_type))
    return;

  bool first_event_listener = !has_event_listener_;
  has_event_listener_ = true;

  if (GetPage() && GetPage()->IsPageVisible()) {
    StartUpdatingIfAttached();
    if (first_event_listener)
      SampleAndCheckConnectedGamepads();
  }
}

void NavigatorGamepad::DidRemoveEventListener(LocalDOMWindow* window,
                                              const AtomicString& event_type) {
  if (IsGamepadEvent(event_type) &&
      !window->HasEventListeners(EventTypeNames::gamepadconnected) &&
      !window->HasEventListeners(EventTypeNames::gamepaddisconnected)) {
    DidRemoveGamepadEventListeners();
  }
}

void NavigatorGamepad::DidRemoveAllEventListeners(LocalDOMWindow*) {
  DidRemoveGamepadEventListeners();
}

void NavigatorGamepad::DidRemoveGamepadEventListeners() {
  has_event_listener_ = false;
  if (dispatch_one_event_runner_)
    dispatch_one_event_runner_->Stop();
  pending_events_.clear();
  StopUpdating();
}

void NavigatorGamepad::SampleAndCheckConnectedGamepads() {
  if (StartUpdatingIfAttached()) {
    if (!gamepads_)
      gamepads_ = GamepadList::Create();
    if (GetPage()->IsPageVisible() && has_event_listener_) {
      if (!gamepads_back_)
        gamepads_back_ = GamepadList::Create();

      // Compare the current sample with the old data and enqueue connection
      // events for any differences.
      SampleGamepads<Gamepad>(gamepads_back_.Get());
      if (CheckConnectedGamepads(gamepads_.Get(), gamepads_back_.Get())) {
        // If we had any disconnected gamepads, we can't overwrite gamepads_
        // because the Gamepad object from the old buffer is reused as the
        // disconnection event and will be overwritten with new data. Instead,
        // recreate the buffer.
        gamepads_ = GamepadList::Create();
      }
      if (!pending_events_.IsEmpty()) {
        DCHECK(dispatch_one_event_runner_);
        dispatch_one_event_runner_->RunAsync();
      }
    }
    SampleGamepads<Gamepad>(gamepads_.Get());
  }
}

bool NavigatorGamepad::CheckConnectedGamepads(GamepadList* old_gamepads,
                                              GamepadList* new_gamepads) {
  int disconnection_count = 0;
  for (unsigned i = 0; i < device::Gamepads::kItemsLengthCap; ++i) {
    Gamepad* old_gamepad = old_gamepads ? old_gamepads->item(i) : nullptr;
    Gamepad* new_gamepad = new_gamepads->item(i);
    bool connected, disconnected;
    CheckConnectedGamepad(old_gamepad, new_gamepad, &connected, &disconnected);

    if (disconnected) {
      old_gamepad->SetConnected(false);
      pending_events_.push_back(old_gamepad);
      disconnection_count++;
    }
    if (connected) {
      pending_events_.push_back(new_gamepad);
    }
  }
  return disconnection_count > 0;
}

void NavigatorGamepad::CheckConnectedGamepad(Gamepad* old_gamepad,
                                             Gamepad* new_gamepad,
                                             bool* gamepad_found,
                                             bool* gamepad_lost) {
  bool old_connected = old_gamepad && old_gamepad->connected();
  bool new_connected = new_gamepad && new_gamepad->connected();
  if (old_gamepad && new_gamepad) {
    HasGamepadConnectionChanged(old_gamepad->id(), new_gamepad->id(),
                                old_connected, new_connected, gamepad_found,
                                gamepad_lost);
    return;
  }

  if (gamepad_found)
    *gamepad_found = new_connected;
  if (gamepad_lost)
    *gamepad_lost = old_connected;
}

void NavigatorGamepad::PageVisibilityChanged() {
  // Inform the embedder whether it needs to provide gamepad data for us.
  bool visible = GetPage()->IsPageVisible();
  if (visible && (has_event_listener_ || gamepads_)) {
    StartUpdatingIfAttached();
  } else {
    StopUpdating();
  }

  if (visible && has_event_listener_)
    SampleAndCheckConnectedGamepads();
}

}  // namespace blink
