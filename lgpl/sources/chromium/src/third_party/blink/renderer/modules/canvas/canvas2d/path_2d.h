/*
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_PATH_2D_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_PATH_2D_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/modules/v8/path_2d_or_string.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix_2d_init.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_path.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

class ExceptionState;

class MODULES_EXPORT Path2D final : public ScriptWrappable, public CanvasPath {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Path2D* Create(Path2DOrString pathorstring) {
    DCHECK(!pathorstring.IsNull());
    if (pathorstring.IsPath2D())
      return MakeGarbageCollected<Path2D>(pathorstring.GetAsPath2D());
    if (pathorstring.IsString())
      return MakeGarbageCollected<Path2D>(pathorstring.GetAsString());
    NOTREACHED();
    return nullptr;
  }
  static Path2D* Create() { return MakeGarbageCollected<Path2D>(); }
  static Path2D* Create(const Path& path) {
    return MakeGarbageCollected<Path2D>(path);
  }

  const Path& GetPath() const { return path_; }

  void addPath(Path2D* path,
               DOMMatrix2DInit* transform,
               ExceptionState& exception_state) {
    DOMMatrixReadOnly* matrix =
        DOMMatrixReadOnly::fromMatrix2D(transform, exception_state);
    if (!matrix || !std::isfinite(matrix->m11()) ||
        !std::isfinite(matrix->m12()) || !std::isfinite(matrix->m21()) ||
        !std::isfinite(matrix->m22()) || !std::isfinite(matrix->m41()) ||
        !std::isfinite(matrix->m42()))
      return;
    path_.AddPath(path->GetPath(), matrix->GetAffineTransform());
  }

  Path2D() : CanvasPath() {}
  Path2D(const Path& path) : CanvasPath(path) {}
  Path2D(Path2D* path) : CanvasPath(path->GetPath()) {}
  Path2D(const String& path_data) : CanvasPath() {
    BuildPathFromString(path_data, path_);
  }
  ~Path2D() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Path2D);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_PATH_2D_H_
