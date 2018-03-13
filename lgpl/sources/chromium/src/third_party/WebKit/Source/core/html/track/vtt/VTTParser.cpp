/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#include "core/html/track/vtt/VTTParser.h"

#include "core/dom/Document.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/Text.h"
#include "core/html/track/vtt/VTTElement.h"
#include "core/html/track/vtt/VTTRegion.h"
#include "core/html/track/vtt/VTTScanner.h"
#include "platform/loader/fetch/TextResourceDecoderOptions.h"
#include "platform/runtime_enabled_features.h"
#include "platform/text/SegmentedString.h"
#include "platform/wtf/DateMath.h"
#include "platform/wtf/text/CharacterNames.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

using namespace HTMLNames;

const unsigned kFileIdentifierLength = 6;

bool VTTParser::ParseFloatPercentageValue(VTTScanner& value_scanner,
                                          float& percentage) {
  float number;
  if (!value_scanner.ScanFloat(number))
    return false;
  // '%' must be present and at the end of the setting value.
  if (!value_scanner.Scan('%'))
    return false;
  if (number < 0 || number > 100)
    return false;
  percentage = number;
  return true;
}

bool VTTParser::ParseFloatPercentageValuePair(VTTScanner& value_scanner,
                                              char delimiter,
                                              FloatPoint& value_pair) {
  float first_coord;
  if (!ParseFloatPercentageValue(value_scanner, first_coord))
    return false;

  if (!value_scanner.Scan(delimiter))
    return false;

  float second_coord;
  if (!ParseFloatPercentageValue(value_scanner, second_coord))
    return false;

  value_pair = FloatPoint(first_coord, second_coord);
  return true;
}

VTTParser::VTTParser(VTTParserClient* client, Document& document)
    : document_(&document),
      state_(kInitial),
      decoder_(TextResourceDecoder::Create(TextResourceDecoderOptions(
          TextResourceDecoderOptions::kPlainTextContent,
          UTF8Encoding()))),
      current_start_time_(0),
      current_end_time_(0),
      client_(client) {}

void VTTParser::GetNewCues(HeapVector<Member<TextTrackCue>>& output_cues) {
  DCHECK(output_cues.IsEmpty());
  output_cues.swap(cue_list_);
}

void VTTParser::ParseBytes(const char* data, size_t length) {
  String text_data = decoder_->Decode(data, length);
  line_reader_.Append(text_data);
  Parse();
}

void VTTParser::Flush() {
  String text_data = decoder_->Flush();
  line_reader_.Append(text_data);
  line_reader_.SetEndOfStream();
  Parse();
  FlushPendingCue();
  region_map_.clear();
}

void VTTParser::Parse() {
  // WebVTT parser algorithm. (5.1 WebVTT file parsing.)
  // Steps 1 - 3 - Initial setup.

  String line;
  while (line_reader_.GetLine(line)) {
    switch (state_) {
      case kInitial:
        // Steps 4 - 9 - Check for a valid WebVTT signature.
        if (!HasRequiredFileIdentifier(line)) {
          if (client_)
            client_->FileFailedToParse();
          return;
        }

        state_ = kHeader;
        break;

      case kHeader:
        // Steps 10 - 14 - Allow a header (comment area) under the WEBVTT line.
        CollectMetadataHeader(line);

        if (line.IsEmpty()) {
          state_ = kId;
          break;
        }

        // Step 15 - Break out of header loop if the line could be a timestamp
        // line.
        if (line.Contains("-->"))
          state_ = RecoverCue(line);

        // Step 16 - Line is not the empty string and does not contain "-->".
        break;

      case kId:
        // Steps 17 - 20 - Allow any number of line terminators, then initialize
        // new cue values.
        if (line.IsEmpty())
          break;

        // Step 21 - Cue creation (start a new cue).
        ResetCueValues();

        // Steps 22 - 25 - Check if this line contains an optional identifier or
        // timing data.
        state_ = CollectCueId(line);
        break;

      case kTimingsAndSettings:
        // Steps 26 - 27 - Discard current cue if the line is empty.
        if (line.IsEmpty()) {
          state_ = kId;
          break;
        }

        // Steps 28 - 29 - Collect cue timings and settings.
        state_ = CollectTimingsAndSettings(line);
        break;

      case kCueText:
        // Steps 31 - 41 - Collect the cue text, create a cue, and add it to the
        // output.
        state_ = CollectCueText(line);
        break;

      case kBadCue:
        // Steps 42 - 48 - Discard lines until an empty line or a potential
        // timing line is seen.
        state_ = IgnoreBadCue(line);
        break;
    }
  }
}

void VTTParser::FlushPendingCue() {
  DCHECK(line_reader_.IsAtEndOfStream());
  // If we're in the CueText state when we run out of data, we emit the pending
  // cue.
  if (state_ == kCueText)
    CreateNewCue();
}

bool VTTParser::HasRequiredFileIdentifier(const String& line) {
  // WebVTT parser algorithm step 6:
  // If input is more than six characters long but the first six characters
  // do not exactly equal "WEBVTT", or the seventh character is not a U+0020
  // SPACE character, a U+0009 CHARACTER TABULATION (tab) character, or a
  // U+000A LINE FEED (LF) character, then abort these steps.
  if (!line.StartsWith("WEBVTT"))
    return false;
  if (line.length() > kFileIdentifierLength) {
    UChar maybe_separator = line[kFileIdentifierLength];
    // The line reader handles the line break characters, so we don't need
    // to check for LF here.
    if (maybe_separator != kSpaceCharacter &&
        maybe_separator != kTabulationCharacter)
      return false;
  }
  return true;
}

void VTTParser::CollectMetadataHeader(const String& line) {
  // WebVTT header parsing (WebVTT parser algorithm step 12)

  // The only currently supported header is the "Region" header.
  if (!RuntimeEnabledFeatures::WebVTTRegionsEnabled())
    return;

  // Step 12.4 If line contains the character ":" (A U+003A COLON), then set
  // metadata's name to the substring of line before the first ":" character and
  // metadata's value to the substring after this character.
  size_t colon_position = line.find(':');
  if (colon_position == kNotFound)
    return;

  String header_name = line.Substring(0, colon_position);

  // Steps 12.5 If metadata's name equals "Region":
  if (header_name == "Region") {
    String header_value = line.Substring(colon_position + 1);
    // Steps 12.5.1 - 12.5.11 Region creation: Let region be a new text track
    // region [...]
    CreateNewRegion(header_value);
  }
}

VTTParser::ParseState VTTParser::CollectCueId(const String& line) {
  if (line.Contains("-->"))
    return CollectTimingsAndSettings(line);
  current_id_ = AtomicString(line);
  return kTimingsAndSettings;
}

VTTParser::ParseState VTTParser::CollectTimingsAndSettings(const String& line) {
  VTTScanner input(line);

  // Collect WebVTT cue timings and settings. (5.3 WebVTT cue timings and
  // settings parsing.)
  // Steps 1 - 3 - Let input be the string being parsed and position be a
  // pointer into input.
  input.SkipWhile<IsASpace>();

  // Steps 4 - 5 - Collect a WebVTT timestamp. If that fails, then abort and
  // return failure. Otherwise, let cue's text track cue start time be the
  // collected time.
  if (!CollectTimeStamp(input, current_start_time_))
    return kBadCue;
  input.SkipWhile<IsASpace>();

  // Steps 6 - 9 - If the next three characters are not "-->", abort and return
  // failure.
  if (!input.Scan("-->"))
    return kBadCue;
  input.SkipWhile<IsASpace>();

  // Steps 10 - 11 - Collect a WebVTT timestamp. If that fails, then abort and
  // return failure. Otherwise, let cue's text track cue end time be the
  // collected time.
  if (!CollectTimeStamp(input, current_end_time_))
    return kBadCue;
  input.SkipWhile<IsASpace>();

  // Step 12 - Parse the WebVTT settings for the cue (conducted in
  // TextTrackCue).
  current_settings_ = input.RestOfInputAsString();
  return kCueText;
}

VTTParser::ParseState VTTParser::CollectCueText(const String& line) {
  // Step 34.
  if (line.IsEmpty()) {
    CreateNewCue();
    return kId;
  }
  // Step 35.
  if (line.Contains("-->")) {
    // Step 39-40.
    CreateNewCue();

    // Step 41 - New iteration of the cue loop.
    return RecoverCue(line);
  }
  if (!current_content_.IsEmpty())
    current_content_.Append('\n');
  current_content_.Append(line);

  return kCueText;
}

VTTParser::ParseState VTTParser::RecoverCue(const String& line) {
  // Step 17 and 21.
  ResetCueValues();

  // Step 22.
  return CollectTimingsAndSettings(line);
}

VTTParser::ParseState VTTParser::IgnoreBadCue(const String& line) {
  if (line.IsEmpty())
    return kId;
  if (line.Contains("-->"))
    return RecoverCue(line);
  return kBadCue;
}

// A helper class for the construction of a "cue fragment" from the cue text.
class VTTTreeBuilder {
  STACK_ALLOCATED();

 public:
  explicit VTTTreeBuilder(Document& document) : document_(&document) {}

  DocumentFragment* BuildFromString(const String& cue_text);

 private:
  void ConstructTreeFromToken(Document&);
  Document& GetDocument() const { return *document_; }

  VTTToken token_;
  Member<ContainerNode> current_node_;
  Vector<AtomicString> language_stack_;
  Member<Document> document_;
};

DocumentFragment* VTTTreeBuilder::BuildFromString(const String& cue_text) {
  // Cue text processing based on
  // 5.4 WebVTT cue text parsing rules, and
  // 5.5 WebVTT cue text DOM construction rules

  DocumentFragment* fragment = DocumentFragment::Create(GetDocument());

  if (cue_text.IsEmpty()) {
    fragment->ParserAppendChild(Text::Create(GetDocument(), ""));
    return fragment;
  }

  current_node_ = fragment;

  VTTTokenizer tokenizer(cue_text);
  language_stack_.clear();

  while (tokenizer.NextToken(token_))
    ConstructTreeFromToken(GetDocument());

  return fragment;
}

DocumentFragment* VTTParser::CreateDocumentFragmentFromCueText(
    Document& document,
    const String& cue_text) {
  VTTTreeBuilder tree_builder(document);
  return tree_builder.BuildFromString(cue_text);
}

void VTTParser::CreateNewCue() {
  VTTCue* cue = VTTCue::Create(*document_, current_start_time_,
                               current_end_time_, current_content_.ToString());
  cue->setId(current_id_);
  cue->ParseSettings(&region_map_, current_settings_);

  cue_list_.push_back(cue);
  if (client_)
    client_->NewCuesParsed();
}

void VTTParser::ResetCueValues() {
  current_id_ = g_empty_atom;
  current_settings_ = g_empty_string;
  current_start_time_ = 0;
  current_end_time_ = 0;
  current_content_.Clear();
}

void VTTParser::CreateNewRegion(const String& header_value) {
  if (header_value.IsEmpty())
    return;

  // Steps 12.5.1 - 12.5.9 - Construct and initialize a WebVTT Region object.
  VTTRegion* region = VTTRegion::Create();
  region->SetRegionSettings(header_value);

  if (region->id().IsEmpty())
    return;
  region_map_.Set(region->id(), region);
}

bool VTTParser::CollectTimeStamp(const String& line, double& time_stamp) {
  VTTScanner input(line);
  return CollectTimeStamp(input, time_stamp);
}

static String SerializeTimeStamp(double time_stamp) {
  uint64_t value = clampTo<uint64_t>(time_stamp * 1000);
  unsigned milliseconds = value % 1000;
  value /= 1000;
  unsigned seconds = value % 60;
  value /= 60;
  unsigned minutes = value % 60;
  unsigned hours = value / 60;
  return String::Format("%02u:%02u:%02u.%03u", hours, minutes, seconds,
                        milliseconds);
}

bool VTTParser::CollectTimeStamp(VTTScanner& input, double& time_stamp) {
  // Collect a WebVTT timestamp (5.3 WebVTT cue timings and settings parsing.)
  // Steps 1 - 4 - Initial checks, let most significant units be minutes.
  enum Mode { kMinutes, kHours };
  Mode mode = kMinutes;

  // Steps 5 - 7 - Collect a sequence of characters that are 0-9.
  // If not 2 characters or value is greater than 59, interpret as hours.
  int value1;
  unsigned value1_digits = input.ScanDigits(value1);
  if (!value1_digits)
    return false;
  if (value1_digits != 2 || value1 > 59)
    mode = kHours;

  // Steps 8 - 11 - Collect the next sequence of 0-9 after ':' (must be 2
  // chars).
  int value2;
  if (!input.Scan(':') || input.ScanDigits(value2) != 2)
    return false;

  // Step 12 - Detect whether this timestamp includes hours.
  int value3;
  if (mode == kHours || input.Match(':')) {
    if (!input.Scan(':') || input.ScanDigits(value3) != 2)
      return false;
  } else {
    value3 = value2;
    value2 = value1;
    value1 = 0;
  }

  // Steps 13 - 17 - Collect next sequence of 0-9 after '.' (must be 3 chars).
  int value4;
  if (!input.Scan('.') || input.ScanDigits(value4) != 3)
    return false;
  if (value2 > 59 || value3 > 59)
    return false;

  // Steps 18 - 19 - Calculate result.
  time_stamp = (value1 * kMinutesPerHour * kSecondsPerMinute) +
               (value2 * kSecondsPerMinute) + value3 +
               (value4 * (1 / kMsPerSecond));
  return true;
}

static VTTNodeType TokenToNodeType(VTTToken& token) {
  switch (token.GetName().length()) {
    case 1:
      if (token.GetName()[0] == 'c')
        return kVTTNodeTypeClass;
      if (token.GetName()[0] == 'v')
        return kVTTNodeTypeVoice;
      if (token.GetName()[0] == 'b')
        return kVTTNodeTypeBold;
      if (token.GetName()[0] == 'i')
        return kVTTNodeTypeItalic;
      if (token.GetName()[0] == 'u')
        return kVTTNodeTypeUnderline;
      break;
    case 2:
      if (token.GetName()[0] == 'r' && token.GetName()[1] == 't')
        return kVTTNodeTypeRubyText;
      break;
    case 4:
      if (token.GetName()[0] == 'r' && token.GetName()[1] == 'u' &&
          token.GetName()[2] == 'b' && token.GetName()[3] == 'y')
        return kVTTNodeTypeRuby;
      if (token.GetName()[0] == 'l' && token.GetName()[1] == 'a' &&
          token.GetName()[2] == 'n' && token.GetName()[3] == 'g')
        return kVTTNodeTypeLanguage;
      break;
  }
  return kVTTNodeTypeNone;
}

void VTTTreeBuilder::ConstructTreeFromToken(Document& document) {
  // http://dev.w3.org/html5/webvtt/#webvtt-cue-text-dom-construction-rules

  switch (token_.GetType()) {
    case VTTTokenTypes::kCharacter: {
      current_node_->ParserAppendChild(
          Text::Create(document, token_.Characters()));
      break;
    }
    case VTTTokenTypes::kStartTag: {
      VTTNodeType node_type = TokenToNodeType(token_);
      if (node_type == kVTTNodeTypeNone)
        break;

      VTTNodeType current_type =
          current_node_->IsVTTElement()
              ? ToVTTElement(current_node_.Get())->WebVTTNodeType()
              : kVTTNodeTypeNone;
      // <rt> is only allowed if the current node is <ruby>.
      if (node_type == kVTTNodeTypeRubyText && current_type != kVTTNodeTypeRuby)
        break;

      VTTElement* child = VTTElement::Create(node_type, &document);
      if (!token_.Classes().IsEmpty())
        child->setAttribute(classAttr, token_.Classes());

      if (node_type == kVTTNodeTypeVoice) {
        child->setAttribute(VTTElement::VoiceAttributeName(),
                            token_.Annotation());
      } else if (node_type == kVTTNodeTypeLanguage) {
        language_stack_.push_back(token_.Annotation());
        child->setAttribute(VTTElement::LangAttributeName(),
                            language_stack_.back());
      }
      if (!language_stack_.IsEmpty())
        child->SetLanguage(language_stack_.back());
      current_node_->ParserAppendChild(child);
      current_node_ = child;
      break;
    }
    case VTTTokenTypes::kEndTag: {
      VTTNodeType node_type = TokenToNodeType(token_);
      if (node_type == kVTTNodeTypeNone)
        break;

      // The only non-VTTElement would be the DocumentFragment root. (Text
      // nodes and PIs will never appear as m_currentNode.)
      if (!current_node_->IsVTTElement())
        break;

      VTTNodeType current_type =
          ToVTTElement(current_node_.Get())->WebVTTNodeType();
      bool matches_current = node_type == current_type;
      if (!matches_current) {
        // </ruby> auto-closes <rt>.
        if (current_type == kVTTNodeTypeRubyText &&
            node_type == kVTTNodeTypeRuby) {
          if (current_node_->parentNode())
            current_node_ = current_node_->parentNode();
        } else {
          break;
        }
      }
      if (node_type == kVTTNodeTypeLanguage)
        language_stack_.pop_back();
      if (current_node_->parentNode())
        current_node_ = current_node_->parentNode();
      break;
    }
    case VTTTokenTypes::kTimestampTag: {
      double parsed_time_stamp;
      if (VTTParser::CollectTimeStamp(token_.Characters(), parsed_time_stamp)) {
        current_node_->ParserAppendChild(ProcessingInstruction::Create(
            document, "timestamp", SerializeTimeStamp(parsed_time_stamp)));
      }
      break;
    }
    default:
      break;
  }
}

void VTTParser::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(client_);
  visitor->Trace(cue_list_);
  visitor->Trace(region_map_);
}

}  // namespace blink
