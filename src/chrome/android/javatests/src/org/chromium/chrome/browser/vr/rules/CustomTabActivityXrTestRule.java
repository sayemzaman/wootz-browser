// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import androidx.test.core.app.ApplicationProvider;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;

/**
 * XR extension of CustomTabActivityTestRule. Applies CustomTabActivityTestRule then opens up a
 * CustomTabActivity to a blank page.
 */
public class CustomTabActivityXrTestRule extends CustomTabActivityTestRule implements XrTestRule {
    @Override
    public Statement apply(final Statement base, final Description desc) {
        return super.apply(
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        startCustomTabActivityWithIntent(
                                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                                        ApplicationProvider.getApplicationContext(),
                                        "about:blank"));
                        base.evaluate();
                    }
                },
                desc);
    }

    @Override
    public @SupportedActivity int getRestriction() {
        return SupportedActivity.CCT;
    }
}
