/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TextCodec_h
#define TextCodec_h

#include <memory>
#include "base/macros.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/Unicode.h"
#include "platform/wtf/text/WTFString.h"

namespace WTF {

class TextEncoding;

// Specifies what will happen when a character is encountered that is
// not encodable in the character set.
enum UnencodableHandling {
  // Encodes the character as an XML entity. For example, U+06DE
  // would be "&#1758;" (0x6DE = 1758 in octal).
  kEntitiesForUnencodables,

  // Encodes the character as en entity as above, but escaped
  // non-alphanumeric characters. This is used in URLs.
  // For example, U+6DE would be "%26%231758%3B".
  kURLEncodedEntitiesForUnencodables,

  // Encodes the character as a CSS entity.  For example U+06DE
  // would be \06de.  See: https://www.w3.org/TR/css-syntax-3/#escaping
  kCSSEncodedEntitiesForUnencodables,
};

typedef char UnencodableReplacementArray[32];

enum FlushBehavior {
  // More bytes are coming, don't flush the codec.
  kDoNotFlush = 0,

  // A fetch has hit EOF. Some codecs handle fetches differently, for compat
  // reasons.
  kFetchEOF,

  // Do a full flush of the codec.
  kDataEOF
};

static_assert(!kDoNotFlush, "DoNotFlush should be falsy");
static_assert(kFetchEOF, "FetchEOF should be truthy");
static_assert(kDataEOF, "DataEOF should be truthy");

class WTF_EXPORT TextCodec {
  USING_FAST_MALLOC(TextCodec);

 public:
  TextCodec() {}
  virtual ~TextCodec();

  String Decode(const char* str,
                size_t length,
                FlushBehavior flush = kDoNotFlush) {
    bool ignored;
    return Decode(str, length, flush, false, ignored);
  }

  virtual String Decode(const char*,
                        size_t length,
                        FlushBehavior,
                        bool stop_on_error,
                        bool& saw_error) = 0;
  virtual CString Encode(const UChar*, size_t length, UnencodableHandling) = 0;
  virtual CString Encode(const LChar*, size_t length, UnencodableHandling) = 0;

  // Fills a null-terminated string representation of the given
  // unencodable character into the given replacement buffer.
  // The length of the string (not including the null) will be returned.
  static int GetUnencodableReplacement(unsigned code_point,
                                       UnencodableHandling,
                                       UnencodableReplacementArray);

  DISALLOW_COPY_AND_ASSIGN(TextCodec);
};

typedef void (*EncodingNameRegistrar)(const char* alias, const char* name);

typedef std::unique_ptr<TextCodec> (
    *NewTextCodecFunction)(const TextEncoding&, const void* additional_data);
typedef void (*TextCodecRegistrar)(const char* name,
                                   NewTextCodecFunction,
                                   const void* additional_data);

}  // namespace WTF

using WTF::TextCodec;

#endif  // TextCodec_h
