// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.displaystyle;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The vertical dimension groups. */
@Retention(RetentionPolicy.SOURCE)
@IntDef({VerticalDisplayStyle.FLAT, VerticalDisplayStyle.REGULAR})
public @interface VerticalDisplayStyle {
    int FLAT = 0;
    int REGULAR = 1;
}
