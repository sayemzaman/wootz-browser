// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.gsa.GSAHelper;
import org.chromium.chrome.browser.historyreport.AppIndexingReporter;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.metrics.VariationsSession;
import org.chromium.chrome.browser.notifications.chime.ChimeDelegate;
import org.chromium.chrome.browser.omaha.RequestGenerator;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmark;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksProviderIterator;
import org.chromium.chrome.browser.password_manager.GooglePasswordManagerUIProvider;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.browser.ui.signin.GoogleActivityController;
import org.chromium.chrome.browser.usage_stats.DigitalWellbeingClient;
import org.chromium.chrome.browser.webapps.GooglePlayWebApkInstallDelegate;
import org.chromium.components.policy.AppRestrictionsProvider;
import org.chromium.components.policy.CombinedPolicyProvider;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.SystemAccountManagerDelegate;
import org.chromium.components.webapps.AppDetailsDelegate;

/**
 * Base class for defining methods where different behavior is required by downstream targets.
 * The correct version of {@link AppHooksImpl} will be determined at compile time via build rules.
 * See http://crbug/560466.
 *
 * Note that new functionality should not be added to AppHooks. Instead the delegate pattern in
 * go/apphooks-migration should be followed to solve this class of problems.
 */
public abstract class AppHooks {
    private static AppHooksImpl sInstanceForTesting;

    /** Sets a mocked instance for testing. */
    public static void setInstanceForTesting(AppHooksImpl instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    public static AppHooks get() {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        // R8 can better optimize if we return a new instance each time.
        return new AppHooksImpl();
    }

    /**
     * Creates a new {@link AccountManagerDelegate}.
     * @return the created {@link AccountManagerDelegate}.
     */
    public AccountManagerDelegate createAccountManagerDelegate() {
        return new SystemAccountManagerDelegate();
    }

    /**
     * @return An instance of AppDetailsDelegate that can be queried about app information for the
     *         App Banner feature.  Will be null if one is unavailable.
     */
    public AppDetailsDelegate createAppDetailsDelegate() {
        return null;
    }

    /**
     * Creates a new {@link AppIndexingReporter}.
     * @return the created {@link AppIndexingReporter}.
     */
    public AppIndexingReporter createAppIndexingReporter() {
        return new AppIndexingReporter();
    }

    /**
     * @return An instance of {@link CustomTabsConnection}. Should not be called
     * outside of {@link CustomTabsConnection#getInstance()}.
     */
    public CustomTabsConnection createCustomTabsConnection() {
        return new CustomTabsConnection();
    }

    /**
     * @return An instance of GoogleActivityController.
     */
    public GoogleActivityController createGoogleActivityController() {
        return new GoogleActivityController();
    }

    /**
     * @return An instance of {@link GSAHelper} that handles the start point of chrome's integration
     *         with GSA.
     */
    public GSAHelper createGsaHelper() {
        return new GSAHelper();
    }

    public InstantAppsHandler createInstantAppsHandler() {
        return new InstantAppsHandler();
    }

    /**
     * @return An instance of {@link GooglePasswordManagerUIProvider}. Will be null if one is not
     *         available.
     */
    public GooglePasswordManagerUIProvider createGooglePasswordManagerUIProvider() {
        return null;
    }

    /**
     * @return An instance of RequestGenerator to be used for Omaha XML creation.  Will be null if
     *         a generator is unavailable.
     */
    public RequestGenerator createOmahaRequestGenerator() {
        return null;
    }

    /**
     * @return a new {@link ProcessInitializationHandler} instance.
     */
    public ProcessInitializationHandler createProcessInitializationHandler() {
        return new ProcessInitializationHandler();
    }

    /**
     * @return An instance of RevenueStats to be installed as a singleton.
     */
    public RevenueStats createRevenueStatsInstance() {
        return new RevenueStats();
    }

    /** Returns a new instance of VariationsSession. */
    public VariationsSession createVariationsSession() {
        return new VariationsSession();
    }

    /** Returns the singleton instance of GooglePlayWebApkInstallDelegate. */
    public GooglePlayWebApkInstallDelegate getGooglePlayWebApkInstallDelegate() {
        return null;
    }

    /**
     * @return An instance of PolicyAuditor that notifies the policy system of the user's activity.
     * Only applicable when the user has a policy active, that is tracking the activity.
     */
    public PolicyAuditor getPolicyAuditor() {
        return null;
    }

    public void registerPolicyProviders(CombinedPolicyProvider combinedProvider) {
        combinedProvider.registerProvider(
                new AppRestrictionsProvider(ContextUtils.getApplicationContext()));
    }

    /**
     * @return An iterator of partner bookmarks.
     */
    @Nullable
    public PartnerBookmark.BookmarkIterator getPartnerBookmarkIterator() {
        return PartnerBookmarksProviderIterator.createIfAvailable();
    }

    /**
     * @return A new {@link DigitalWellbeingClient} instance.
     */
    public DigitalWellbeingClient createDigitalWellbeingClient() {
        return new DigitalWellbeingClient();
    }

    /** Returns a new {@link TrustedVaultClient.Backend} instance. */
    public TrustedVaultClient.Backend createSyncTrustedVaultClientBackend() {
        return new TrustedVaultClient.EmptyBackend();
    }

    /** Returns the URL to the WebAPK creation/update server. */
    public String getWebApkServerUrl() {
        return "";
    }

    /** Returns a Chime Delegate if the chime module is defined. */
    public ChimeDelegate getChimeDelegate() {
        return new ChimeDelegate();
    }

    public String getDefaultQueryTilesServerUrl() {
        return "";
    }

    public void registerProtoExtensions() {}

    // Stop! Do not add new methods to AppHooks anymore. Follow go/apphooks-migration instead.
}
