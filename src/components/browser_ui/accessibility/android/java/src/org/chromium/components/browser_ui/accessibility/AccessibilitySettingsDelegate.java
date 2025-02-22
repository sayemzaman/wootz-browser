// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;
import android.app.Activity;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * An interface implemented by the embedder that allows the Accessibility Settings UI to access
 * embedder-specific logic.
 */
public interface AccessibilitySettingsDelegate {
    /** An interface to control a single integer preference. */
    interface IntegerPreferenceDelegate {
        /** @return int - Current value of the preference of this instance. */
        int getValue();

        /**
         * Sets a new value for the preference of this instance.
         * @param value
         */
        void setValue(int value);
    }
    interface BooleanPreferenceDelegate {
        /**
         * @return whether the preference is enabled.
         */
        boolean isEnabled();

        /**
         * Called when the preference value is changed.
         */
        void setEnabled(boolean value);
    }
    void requestRestart(Activity activity);

    BooleanPreferenceDelegate getMoveTopToolbarToBottomDelegate();
    BooleanPreferenceDelegate getDisableToolbarSwipeUpDelegate();

    /** @return The BrowserContextHandle that should be used to read and update settings. */
    BrowserContextHandle getBrowserContextHandle();

    /**
     * @return the InterPreferenceDelegate instance that should be used for reading and setting the
     * text size contrast value for accessibility settings. Return null to omit the preference.
     */
    IntegerPreferenceDelegate getTextSizeContrastAccessibilityDelegate();
}
