/*
 * Copyright (C) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AXInlineTextBox_h
#define AXInlineTextBox_h

#include "core/layout/line/AbstractInlineTextBox.h"
#include "modules/accessibility/AXObject.h"

namespace blink {

class Node;
class AXObjectCacheImpl;

class AXInlineTextBox final : public AXObject {
  WTF_MAKE_NONCOPYABLE(AXInlineTextBox);

 private:
  AXInlineTextBox(scoped_refptr<AbstractInlineTextBox>, AXObjectCacheImpl&);

 public:
  static AXInlineTextBox* Create(scoped_refptr<AbstractInlineTextBox>,
                                 AXObjectCacheImpl&);

 protected:
  void Init() override;
  void Detach() override;
  bool IsDetached() const override { return !inline_text_box_; }
  bool IsAXInlineTextBox() const override { return true; }

 public:
  AccessibilityRole RoleValue() const override { return kInlineTextBoxRole; }
  String GetName(AXNameFrom&,
                 AXObject::AXObjectVector* name_objects) const override;
  void TextCharacterOffsets(Vector<int>&) const override;
  void GetWordBoundaries(Vector<AXRange>&) const override;
  void GetRelativeBounds(AXObject** out_container,
                         FloatRect& out_bounds_in_container,
                         SkMatrix44& out_container_transform,
                         bool* clips_children = nullptr) const override;
  AXObject* ComputeParent() const override;
  AccessibilityTextDirection GetTextDirection() const override;
  Node* GetNode() const override;
  AXObject* NextOnLine() const override;
  AXObject* PreviousOnLine() const override;

 private:
  scoped_refptr<AbstractInlineTextBox> inline_text_box_;

  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
};

}  // namespace blink

#endif  // AXInlineTextBox_h
