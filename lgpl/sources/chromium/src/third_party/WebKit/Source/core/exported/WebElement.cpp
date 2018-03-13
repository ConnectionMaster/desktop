/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/web/WebElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Element.h"
#include "core/dom/Element.h"
#include "core/editing/EditingUtilities.h"
#include "core/html/custom/V0CustomElementProcessingStack.h"
#include "core/html/forms/TextControlElement.h"
#include "core/html_names.h"
#include "platform/graphics/Image.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebRect.h"
#include <v8.h>

namespace blink {

using namespace HTMLNames;

WebElement WebElement::FromV8Value(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value) {
  Element* element = V8Element::ToImplWithTypeCheck(isolate, value);
  return WebElement(element);
}

bool WebElement::IsFormControlElement() const {
  return ConstUnwrap<Element>()->IsFormControlElement();
}

// TODO(dglazkov): Remove. Consumers of this code should use
// Node:hasEditableStyle.  http://crbug.com/612560
bool WebElement::IsEditable() const {
  const Element* element = ConstUnwrap<Element>();

  element->GetDocument().UpdateStyleAndLayoutTree();
  if (HasEditableStyle(*element))
    return true;

  if (element->IsTextControl()) {
    if (!ToTextControlElement(element)->IsDisabledOrReadOnly())
      return true;
  }

  return EqualIgnoringASCIICase(element->getAttribute(roleAttr), "textbox");
}

WebString WebElement::TagName() const {
  return ConstUnwrap<Element>()->tagName();
}

bool WebElement::HasHTMLTagName(const WebString& tag_name) const {
  // How to create                     class              nodeName localName
  // createElement('input')            HTMLInputElement   INPUT    input
  // createElement('INPUT')            HTMLInputElement   INPUT    input
  // createElementNS(xhtmlNS, 'input') HTMLInputElement   INPUT    input
  // createElementNS(xhtmlNS, 'INPUT') HTMLUnknownElement INPUT    INPUT
  const Element* element = ConstUnwrap<Element>();
  return HTMLNames::xhtmlNamespaceURI == element->namespaceURI() &&
         element->localName() == String(tag_name).DeprecatedLower();
}

bool WebElement::HasAttribute(const WebString& attr_name) const {
  return ConstUnwrap<Element>()->hasAttribute(attr_name);
}

WebString WebElement::GetAttribute(const WebString& attr_name) const {
  return ConstUnwrap<Element>()->getAttribute(attr_name);
}

void WebElement::SetAttribute(const WebString& attr_name,
                              const WebString& attr_value) {
  // TODO: Custom element callbacks need to be called on WebKit API methods that
  // mutate the DOM in any way.
  V0CustomElementProcessingStack::CallbackDeliveryScope
      deliver_custom_element_callbacks;
  Unwrap<Element>()->setAttribute(attr_name, attr_value,
                                  IGNORE_EXCEPTION_FOR_TESTING);
}

unsigned WebElement::AttributeCount() const {
  if (!ConstUnwrap<Element>()->hasAttributes())
    return 0;
  return ConstUnwrap<Element>()->Attributes().size();
}

WebString WebElement::AttributeLocalName(unsigned index) const {
  if (index >= AttributeCount())
    return WebString();
  return ConstUnwrap<Element>()->Attributes().at(index).LocalName();
}

WebString WebElement::AttributeValue(unsigned index) const {
  if (index >= AttributeCount())
    return WebString();
  return ConstUnwrap<Element>()->Attributes().at(index).Value();
}

WebString WebElement::TextContent() const {
  return ConstUnwrap<Element>()->textContent();
}

void WebElement::RequestDetachedView() {
  Element* element = Unwrap<Element>();
  element->RequestDetachedView();
}

void WebElement::ReleaseDetachedView() {
  Element* element = Unwrap<Element>();
  element->ReleaseDetachedView();
}

bool WebElement::HasDetachedView() {
  Element* element = Unwrap<Element>();
  return element->HasDetachedView();
}

bool WebElement::IsVideoDetachAllowed() {
  Element* element = Unwrap<Element>();
  return element->IsVideoDetachAllowed();
}

void WebElement::InvokeDetachedViewAction(const WebString& action) {
  Element* element = Unwrap<Element>();
  return element->InvokeDetachedViewAction(action);
}

void WebElement::UpdateDetachedViewSubtitle(const WebString& text) {
  Element* element = Unwrap<Element>();
  return element->UpdateDetachedViewSubtitle(text);
}

void WebElement::RequestVRPlayback() {
  Element* element = Unwrap<Element>();
  element->RequestVRPlayback();
}

void WebElement::ExitVRPlayback() {
  Element* element = Unwrap<Element>();
  element->ExitVRPlayback();
}

bool WebElement::HasVRPlayback() {
  Element* element = Unwrap<Element>();
  return element->HasVRPlayback();
}

bool WebElement::IsVRPlaybackAllowed() {
  Element* element = Unwrap<Element>();
  return element->IsVRPlaybackAllowed();
}

WebString WebElement::InnerHTML() const {
  return ConstUnwrap<Element>()->InnerHTMLAsString();
}

bool WebElement::HasNonEmptyLayoutSize() const {
  return ConstUnwrap<Element>()->HasNonEmptyLayoutSize();
}

WebRect WebElement::BoundsInViewport() const {
  return ConstUnwrap<Element>()->BoundsInViewport();
}

WebImage WebElement::ImageContents() {
  if (IsNull())
    return WebImage();

  return WebImage(Unwrap<Element>()->ImageContents());
}

WebElement::WebElement(Element* elem) : WebNode(elem) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebElement, IsElementNode());

WebElement& WebElement::operator=(Element* elem) {
  private_ = elem;
  return *this;
}

WebElement::operator Element*() const {
  return ToElement(private_.Get());
}

}  // namespace blink
