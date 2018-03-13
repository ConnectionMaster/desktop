/*
 * Copyright (c) 2013, Opera Software AS. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software AS nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GlobalEventHandlers_h
#define GlobalEventHandlers_h

#include "core/dom/events/EventTarget.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class GlobalEventHandlers {
  STATIC_ONLY(GlobalEventHandlers);

 public:
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(abort);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(auxclick);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(blur);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(cancel);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(canplay);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(canplaythrough);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(change);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(click);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(close);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(contextmenu);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(cuechange);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(dblclick);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(drag);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(dragend);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(dragenter);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(dragleave);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(dragover);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(dragstart);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(drop);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(durationchange);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(emptied);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(ended);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(error);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(focus);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(gotpointercapture);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(input);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(invalid);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(keydown);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(keypress);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(keyup);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(load);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(loadeddata);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(loadedmetadata);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(loadstart);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(lostpointercapture);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mousedown);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mouseenter);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mouseleave);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mousemove);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mouseout);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mouseover);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mouseup);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(mousewheel);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(operacustomcontrol);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(operadetachedviewchange);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(operavrplayerchange);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(operavrplayererror);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pause);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(play);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(playing);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointercancel);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerdown);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerenter);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerleave);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointermove);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerout);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerover);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(pointerup);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(progress);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(ratechange);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(reset);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(resize);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(scroll);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(seeked);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(seeking);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(select);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(stalled);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(submit);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(suspend);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(timeupdate);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(toggle);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(touchcancel);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(touchend);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(touchmove);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(touchstart);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(volumechange);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(waiting);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(wheel);
};

}  // namespace

#endif
