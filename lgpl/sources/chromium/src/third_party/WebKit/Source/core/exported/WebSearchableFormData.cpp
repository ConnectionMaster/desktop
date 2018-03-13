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

#include "public/web/WebSearchableFormData.h"

#include "core/dom/Document.h"
#include "core/html/forms/FormData.h"
#include "core/html/forms/HTMLFormControlElement.h"
#include "core/html/forms/HTMLFormElement.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/html/forms/HTMLOptionElement.h"
#include "core/html/forms/HTMLSelectElement.h"
#include "core/html_names.h"
#include "core/input_type_names.h"
#include "platform/network/FormDataEncoder.h"
#include "platform/wtf/text/TextEncoding.h"
#include "public/platform/WebRuntimeFeatures.h"
#include "public/web/WebFormElement.h"
#include "public/web/WebInputElement.h"

namespace blink {

using namespace HTMLNames;

namespace {

// Gets the encoding for the form.
// TODO(tkent): Use FormDataEncoder::encodingFromAcceptCharset().
void GetFormEncoding(const HTMLFormElement& form, WTF::TextEncoding* encoding) {
  String str(form.FastGetAttribute(HTMLNames::accept_charsetAttr));
  str.Replace(',', ' ');
  Vector<String> charsets;
  str.Split(' ', charsets);
  for (const String& charset : charsets) {
    *encoding = WTF::TextEncoding(charset);
    if (encoding->IsValid())
      return;
  }
  if (form.GetDocument().Loader())
    *encoding = WTF::TextEncoding(form.GetDocument().Encoding());
}

// If the form does not have an activated submit button, the first submit
// button is returned.
HTMLFormControlElement* ButtonToActivate(const HTMLFormElement& form) {
  HTMLFormControlElement* first_submit_button = nullptr;
  for (auto& element : form.ListedElements()) {
    if (!element->IsFormControlElement())
      continue;
    HTMLFormControlElement* control = ToHTMLFormControlElement(element);
    if (control->IsActivatedSubmit()) {
      // There's a button that is already activated for submit, return
      // nullptr.
      return nullptr;
    }
    if (!first_submit_button && control->IsSuccessfulSubmitButton())
      first_submit_button = control;
  }
  return first_submit_button;
}

// Returns true if the selected state of all the options matches the default
// selected state.
bool IsSelectInDefaultState(const HTMLSelectElement& select) {
  if (select.IsMultiple() || select.size() > 1) {
    for (const auto& option_element : select.GetOptionList()) {
      if (option_element->Selected() !=
          option_element->FastHasAttribute(selectedAttr))
        return false;
    }
    return true;
  }

  // The select is rendered as a combobox (called menulist in WebKit). At
  // least one item is selected, determine which one.
  HTMLOptionElement* initial_selected = nullptr;
  for (const auto& option_element : select.GetOptionList()) {
    if (option_element->FastHasAttribute(selectedAttr)) {
      // The page specified the option to select.
      initial_selected = option_element;
      break;
    }
    if (!initial_selected)
      initial_selected = option_element;
  }
  return !initial_selected || initial_selected->Selected();
}

// Returns true if the form element is in its default state, false otherwise.
// The default state is the state of the form element on initial load of the
// page, and varies depending upon the form element. For example, a checkbox is
// in its default state if the checked state matches the state of the checked
// attribute.
bool IsInDefaultState(const HTMLFormControlElement& form_element) {
  if (auto* input = ToHTMLInputElementOrNull(form_element)) {
    if (input->type() == InputTypeNames::checkbox ||
        input->type() == InputTypeNames::radio)
      return input->checked() == input->FastHasAttribute(checkedAttr);
  } else if (auto* select = ToHTMLSelectElementOrNull(form_element)) {
    return IsSelectInDefaultState(*select);
  }
  return true;
}

// Look for a suitable search text field in a given HTMLFormElement
// Return nothing if one of those items are found:
//  - A text area field
//  - A file upload field
//  - A Password field
//  - More than one text field
HTMLInputElement* FindSuitableSearchInputElement(const HTMLFormElement& form) {
  HTMLInputElement* text_element = nullptr;
  for (const auto& item : form.ListedElements()) {
    if (!item->IsFormControlElement())
      continue;

    HTMLFormControlElement& control = ToHTMLFormControlElement(*item);

    if (control.IsDisabledFormControl() || control.GetName().IsNull())
      continue;

    if (!IsInDefaultState(control) || IsHTMLTextAreaElement(control))
      return nullptr;

    if (IsHTMLInputElement(control) && control.willValidate()) {
      const HTMLInputElement& input = ToHTMLInputElement(control);

      // Return nothing if a file upload field or a password field are
      // found.
      if (input.type() == InputTypeNames::file ||
          input.type() == InputTypeNames::password)
        return nullptr;

      if (input.IsTextField()) {
        if (text_element) {
          // The auto-complete bar only knows how to fill in one
          // value.  This form has multiple fields; don't treat it as
          // searchable.
          return nullptr;
        }
        text_element = ToHTMLInputElement(&control);
      }
    }
  }
  return text_element;
}

// Build a search string based on a given HTMLFormElement and HTMLInputElement
//
// Search string output example from www.google.com:
// "hl=en&source=hp&biw=1085&bih=854&q={searchTerms}&btnG=Google+Search&aq=f&aqi=&aql=&oq="
//
// Return false if the provided HTMLInputElement is not found in the form
bool BuildSearchString(const HTMLFormElement& form,
                       Vector<char>* encoded_string,
                       const WTF::TextEncoding& encoding,
                       const HTMLInputElement* text_element) {
  bool is_element_found = false;
  for (const auto& item : form.ListedElements()) {
    if (!item->IsFormControlElement())
      continue;

    HTMLFormControlElement& control = ToHTMLFormControlElement(*item);
    if (control.IsDisabledFormControl() || control.GetName().IsNull())
      continue;

    FormData* form_data = FormData::Create(encoding);
    control.AppendToFormData(*form_data);

    for (const auto& entry : form_data->Entries()) {
      if (!encoded_string->IsEmpty())
        encoded_string->push_back('&');
      FormDataEncoder::EncodeStringAsFormData(*encoded_string, entry->name(),
                                              FormDataEncoder::kNormalizeCRLF);
      encoded_string->push_back('=');
      if (&control == text_element) {
        encoded_string->Append("{searchTerms}", 13);
        is_element_found = true;
      } else {
        FormDataEncoder::EncodeStringAsFormData(
            *encoded_string, entry->Value(), FormDataEncoder::kNormalizeCRLF);
      }
    }
  }
  return is_element_found;
}

}  // namespace

WebSearchableFormData::WebSearchableFormData(
    const WebFormElement& form,
    const WebInputElement& selected_input_element) {
  HTMLFormElement* form_element = static_cast<HTMLFormElement*>(form);
  HTMLInputElement* input_element =
      static_cast<HTMLInputElement*>(selected_input_element);

  bool is_post_form = DeprecatedEqualIgnoringCase(
      form_element->getAttribute(methodAttr), "post");

  if (is_post_form &&
      !WebRuntimeFeatures::IsWebSearchableFormDataPOSTSupportEnabled())
    return;

  WTF::TextEncoding encoding;
  GetFormEncoding(*form_element, &encoding);
  if (!encoding.IsValid()) {
    // Need a valid encoding to encode the form elements.
    // If the encoding isn't found webkit ends up replacing the params with
    // empty strings. So, we don't try to do anything here.
    return;
  }

  // Look for a suitable search text field in the form when a
  // selectedInputElement is not provided.
  if (!input_element) {
    input_element = FindSuitableSearchInputElement(*form_element);

    // Return if no suitable text element has been found.
    if (!input_element)
      return;
  }

  HTMLFormControlElement* first_submit_button = ButtonToActivate(*form_element);
  if (first_submit_button) {
    // The form does not have an active submit button, make the first button
    // active. We need to do this, otherwise the URL will not contain the
    // name of the submit button.
    first_submit_button->SetActivatedSubmit(true);
  }

  Vector<char> encoded_string;
  bool is_valid_search_string = BuildSearchString(
      *form_element, &encoded_string, encoding, input_element);

  if (first_submit_button)
    first_submit_button->SetActivatedSubmit(false);

  // Return if the search string is not valid.
  if (!is_valid_search_string)
    return;

  String action(form_element->Action());
  KURL url(
      form_element->GetDocument().CompleteURL(action.IsNull() ? "" : action));
  scoped_refptr<EncodedFormData> form_data =
      EncodedFormData::Create(encoded_string);
  if (is_post_form)
    post_data_ = form_data->FlattenToString();
  else
    url.SetQuery(form_data->FlattenToString());

  url_ = url;
  encoding_ = String(encoding.GetName());
}

}  // namespace blink
