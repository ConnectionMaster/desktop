// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encoding/text_encoder_stream.h"

#include <stdint.h>
#include <string.h>

#include <memory>
#include <utility>

#include "base/optional.h"
#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller_interface.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "v8/include/v8.h"

namespace blink {

class TextEncoderStream::Transformer final : public TransformStreamTransformer {
 public:
  explicit Transformer(ScriptState* script_state)
      : encoder_(NewTextCodec(WTF::TextEncoding("utf-8"))),
        script_state_(script_state) {}

  // Implements the "encode and enqueue a chunk" algorithm. For efficiency, only
  // the characters at the end of chunks are special-cased.
  void Transform(v8::Local<v8::Value> chunk,
                 TransformStreamDefaultControllerInterface* controller,
                 ExceptionState& exception_state) override {
    V8StringResource<> input_resource = chunk;
    if (!input_resource.Prepare(script_state_->GetIsolate(), exception_state))
      return;
    const String input = input_resource;
    if (input.IsEmpty())
      return;

    const base::Optional<UChar> high_surrogate = pending_high_surrogate_;
    pending_high_surrogate_ = base::nullopt;
    std::string prefix;
    std::string result;
    if (input.Is8Bit()) {
      if (high_surrogate.has_value()) {
        // An 8-bit code unit can never be part of an astral character, so no
        // check is needed.
        prefix = ReplacementCharacterInUtf8();
      }
      result = encoder_->Encode(input.Characters8(), input.length(),
                                WTF::kNoUnencodables);
    } else {
      bool have_output =
          Encode16BitString(input, high_surrogate, &prefix, &result);
      if (!have_output)
        return;
    }

    DOMUint8Array* array =
        CreateDOMUint8ArrayFromTwoStdStringsConcatenated(prefix, result);
    controller->Enqueue(ToV8(array, script_state_), exception_state);
  }

  // Implements the "encode and flush" algorithm.
  void Flush(TransformStreamDefaultControllerInterface* controller,
             ExceptionState& exception_state) override {
    if (!pending_high_surrogate_.has_value())
      return;

    const std::string replacement_character = ReplacementCharacterInUtf8();
    const uint8_t* u8buffer =
        reinterpret_cast<const uint8_t*>(replacement_character.c_str());
    controller->Enqueue(ToV8(DOMUint8Array::Create(
                                 u8buffer, static_cast<unsigned int>(
                                               replacement_character.length())),
                             script_state_),
                        exception_state);
  }

  ScriptState* GetScriptState() override { return script_state_; }

  void Trace(Visitor* visitor) override {
    visitor->Trace(script_state_);
    TransformStreamTransformer::Trace(visitor);
  }

 private:
  static std::string ReplacementCharacterInUtf8() {
    constexpr char kRawBytes[] = {0xEF, 0xBF, 0xBD};
    return std::string(kRawBytes, sizeof(kRawBytes));
  }

  static DOMUint8Array* CreateDOMUint8ArrayFromTwoStdStringsConcatenated(
      const std::string& string1,
      const std::string& string2) {
    const wtf_size_t length1 = static_cast<wtf_size_t>(string1.length());
    const wtf_size_t length2 = static_cast<wtf_size_t>(string2.length());
    DOMUint8Array* const array = DOMUint8Array::Create(length1 + length2);
    if (length1 > 0)
      memcpy(array->Data(), string1.c_str(), length1);
    if (length2 > 0)
      memcpy(array->Data() + length1, string2.c_str(), length2);
    return array;
  }

  // Returns true if either |*prefix| or |*result| have been set to a non-empty
  // value.
  bool Encode16BitString(const String& input,
                         base::Optional<UChar> high_surrogate,
                         std::string* prefix,
                         std::string* result) {
    const UChar* begin = input.Characters16();
    const UChar* end = input.Characters16() + input.length();
    DCHECK_GT(end, begin);
    if (high_surrogate.has_value()) {
      if (*begin >= 0xDC00 && *begin <= 0xDFFF) {
        const UChar astral_character[2] = {high_surrogate.value(), *begin};
        // Third argument is ignored, as above.
        *prefix =
            encoder_->Encode(astral_character, base::size(astral_character),
                             WTF::kNoUnencodables);
        ++begin;
        if (begin == end)
          return true;
      } else {
        *prefix = ReplacementCharacterInUtf8();
      }
    }

    const UChar final_token = *(end - 1);
    if (final_token >= 0xD800 && final_token <= 0xDBFF) {
      pending_high_surrogate_ = final_token;
      --end;
      if (begin == end)
        return prefix->length() != 0;
    }

    // Third argument is ignored, as above.
    *result = encoder_->Encode(begin, static_cast<wtf_size_t>(end - begin),
                               WTF::kEntitiesForUnencodables);
    DCHECK_NE(result->length(), 0u);
    return true;
  }

  std::unique_ptr<WTF::TextCodec> encoder_;
  // There is no danger of ScriptState leaking across worlds because a
  // TextEncoderStream can only be accessed from the world that created it.
  Member<ScriptState> script_state_;
  base::Optional<UChar> pending_high_surrogate_;

  DISALLOW_COPY_AND_ASSIGN(Transformer);
};

TextEncoderStream* TextEncoderStream::Create(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  return MakeGarbageCollected<TextEncoderStream>(script_state, exception_state);
}

TextEncoderStream::~TextEncoderStream() = default;

String TextEncoderStream::encoding() const {
  return "utf-8";
}

ReadableStream* TextEncoderStream::readable() const {
  return transform_->Readable();
}

WritableStream* TextEncoderStream::writable() const {
  return transform_->Writable();
}

void TextEncoderStream::Trace(Visitor* visitor) {
  visitor->Trace(transform_);
  ScriptWrappable::Trace(visitor);
}

TextEncoderStream::TextEncoderStream(ScriptState* script_state,
                                     ExceptionState& exception_state)
    : transform_(MakeGarbageCollected<TransformStream>()) {
  transform_->Init(MakeGarbageCollected<Transformer>(script_state),
                   script_state, exception_state);
}

}  // namespace blink
