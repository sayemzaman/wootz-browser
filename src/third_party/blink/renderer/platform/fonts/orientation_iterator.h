// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_ORIENTATION_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_ORIENTATION_ITERATOR_H_

#include "third_party/blink/renderer/platform/fonts/font_orientation.h"
#include "third_party/blink/renderer/platform/fonts/script_run_iterator.h"
#include "third_party/blink/renderer/platform/fonts/utf16_text_iterator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PLATFORM_EXPORT OrientationIterator {
  STACK_ALLOCATED();

 public:
  enum RenderOrientation {
    kOrientationKeep,
    kOrientationRotateSideways,
    kOrientationInvalid
  };

  OrientationIterator(const UChar* buffer,
                      unsigned buffer_size,
                      FontOrientation run_orientation);
  OrientationIterator(const OrientationIterator&) = delete;
  OrientationIterator& operator=(const OrientationIterator&) = delete;

  bool Consume(unsigned* orientation_limit, RenderOrientation*);

 private:
  UTF16TextIterator utf16_iterator_;
  bool at_end_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_ORIENTATION_ITERATOR_H_
