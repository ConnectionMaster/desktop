/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
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

#include "core/animation/AnimationClock.h"

#include <math.h>
#include "platform/wtf/Time.h"

namespace {

// FIXME: This is an approximation of time between frames, used when
// ticking the animation clock outside of animation frame callbacks.
// Ideally this would be generated by the compositor.
const double approximateFrameTime = 1 / 60.0;
}

namespace blink {

unsigned AnimationClock::currently_running_task_ = 0;

void AnimationClock::UpdateTime(double time) {
  if (time > time_)
    time_ = time;
  task_for_which_time_was_calculated_ = currently_running_task_;
}

double AnimationClock::CurrentTime() {
  if (monotonically_increasing_time_ &&
      task_for_which_time_was_calculated_ != currently_running_task_) {
    const double current_time = monotonically_increasing_time_();
    if (time_ < current_time) {
      // Advance to the first estimated frame after the current time.
      const double frame_shift =
          fmod(current_time - time_, approximateFrameTime);
      const double new_time =
          current_time + (approximateFrameTime - frame_shift);
      DCHECK_GE(new_time, current_time);
      DCHECK_LE(new_time, current_time + approximateFrameTime);
      UpdateTime(new_time);
    } else {
      task_for_which_time_was_calculated_ = currently_running_task_;
    }
  }
  return time_;
}

void AnimationClock::ResetTimeForTesting(double time) {
  time_ = time;
  task_for_which_time_was_calculated_ = 0;
  currently_running_task_ = 0;
}

}  // namespace blink
