// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/option_list.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"

namespace blink {

void OptionListIterator::Advance(HTMLOptionElement* previous) {
  // This function returns only
  // - An OPTION child of select_, or
  // - An OPTION child of an OPTGROUP child of select_.
  // - An OPTION descendant of a DATALIST child of select_ if StylableSelect is
  // enabled.

  Element* current;
  if (previous) {
    DCHECK_EQ(previous->OwnerSelectElement(), select_);
    current = ElementTraversal::NextSkippingChildren(*previous, select_);
  } else {
    current = ElementTraversal::FirstChild(*select_);
  }
  while (current) {
    if (auto* option = DynamicTo<HTMLOptionElement>(current)) {
      current_ = option;
      return;
    }
    if (IsA<HTMLOptGroupElement>(current) && current->parentNode() == select_) {
      if ((current_ = Traversal<HTMLOptionElement>::FirstChild(*current)))
        return;
    }
    if (RuntimeEnabledFeatures::StylableSelectEnabled()) {
      // TODO(crbug.com/1511354): Consider using a flat tree traversal here
      // instead of a node traversal. That would probably also require changing
      // HTMLOptionsCollection to support flat tree traversals as well.
      if (auto* datalist = select_->FirstChildDatalist()) {
        // TODO(crbug.com/1511354): We shouldn't have to call IsDescendantOf
        // here since we are already doing a tree traversal. This increases
        // runtime.
        if (current == datalist || (current->IsDescendantOf(datalist) &&
                                    !IsA<HTMLSelectElement>(current))) {
          current = ElementTraversal::Next(*current, select_);
          continue;
        }
      }
    }
    current = ElementTraversal::NextSkippingChildren(*current, select_);
  }
  current_ = nullptr;
}

}  // namespace blink
