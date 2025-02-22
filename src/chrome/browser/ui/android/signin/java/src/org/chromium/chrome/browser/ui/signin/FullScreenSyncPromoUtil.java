// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Helper class responsible of launching the full screen sync promo, i.e. {@link
 * SyncConsentActivity}. After UNO, this will launch the re-FRE with {@link
 * SigninAndHistoryOptInActivity}.
 */
// TODO(b/41493788): Consider renaming this to UpgradePromoUtil.
public final class FullScreenSyncPromoUtil {
    /**
     * Launches the {@link SyncConsentActivity} if it needs to be displayed.
     *
     * @param context The {@link Context} to launch the {@link SyncConsentActivity}.
     * @param profile The active user profile.
     * @param syncConsentActivityLauncher launcher used to launch the {@link SyncConsentActivity}.
     * @param signinAndHistoryOptInActivityLauncher launcher used to launch the {@link
     *     SigninAndHistoryOptInActivity}.
     * @param currentMajorVersion The current major version of Chrome.
     * @return Whether the signin promo is shown.
     */
    public static boolean launchPromoIfNeeded(
            Context context,
            Profile profile,
            SyncConsentActivityLauncher syncConsentActivityLauncher,
            SigninAndHistoryOptInActivityLauncher signinAndHistoryOptInActivityLauncher,
            final int currentMajorVersion) {
        final SigninPreferencesManager prefManager = SigninPreferencesManager.getInstance();
        if (shouldLaunchPromo(profile, prefManager, currentMajorVersion)) {
            if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                    && !BuildInfo.getInstance().isAutomotive) {
                signinAndHistoryOptInActivityLauncher.launchUpgradePromoActivityIfAllowed(
                        context, profile);
            } else {
                syncConsentActivityLauncher.launchActivityIfAllowed(
                        context, SigninAccessPoint.SIGNIN_PROMO);
            }
            prefManager.setSigninPromoLastShownVersion(currentMajorVersion);
            final List<CoreAccountInfo> coreAccountInfos =
                    AccountUtils.getCoreAccountInfosIfFulfilledOrEmpty(
                            AccountManagerFacadeProvider.getInstance().getCoreAccountInfos());
            prefManager.setSigninPromoLastAccountEmails(
                    new HashSet<>(AccountUtils.toAccountEmails(coreAccountInfos)));
            return true;
        }
        return false;
    }

    // TODO(b/41493788): Check the PRD to see if the conditions on showing this promo have changed.
    private static boolean shouldLaunchPromo(
            Profile profile, SigninPreferencesManager prefManager, final int currentMajorVersion) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FORCE_STARTUP_SIGNIN_PROMO)) {
            return true;
        }
        final int lastPromoMajorVersion = prefManager.getSigninPromoLastShownVersion();
        if (lastPromoMajorVersion == 0) {
            prefManager.setSigninPromoLastShownVersion(currentMajorVersion);
            return false;
        }

        if (currentMajorVersion < lastPromoMajorVersion + 2) {
            // Promo can be shown at most once every 2 Chrome major versions.
            return false;
        }

        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (identityManager.getPrimaryAccountInfo(ConsentLevel.SYNC) != null) {
            // Don't show if user is signed in and syncing.
            return false;
        }

        if (!TextUtils.isEmpty(
                UserPrefs.get(profile).getString(Pref.GOOGLE_SERVICES_LAST_SYNCING_USERNAME))) {
            // Don't show if user has manually signed out.
            return false;
        }

        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        final List<CoreAccountInfo> coreAccountInfos =
                AccountUtils.getCoreAccountInfosIfFulfilledOrEmpty(
                        accountManagerFacade.getCoreAccountInfos());
        if (coreAccountInfos.isEmpty()) {
            // Don't show if the account list isn't available yet or there are no accounts in it.
            return false;
        }

        // TODO(crbug.com/40928908): Use IdentityManager.findExtendedAccountInfoByAccountId()
        // instead.
        final @Nullable AccountInfo firstAccount =
                identityManager.findExtendedAccountInfoByEmailAddress(
                        coreAccountInfos.get(0).getEmail());
        if (!(firstAccount != null
                && firstAccount
                                .getAccountCapabilities()
                                .canShowHistorySyncOptInsWithoutMinorModeRestrictions()
                        == Tribool.TRUE)) {
            // Show promo only when CanShowHistorySyncOptInsWithoutMinorModeRestrictions capability
            // for the first account
            // is fetched and true.
            return false;
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS)) {
            return false;
        }

        final List<String> currentAccountEmails = AccountUtils.toAccountEmails(coreAccountInfos);
        final Set<String> previousAccountEmails = prefManager.getSigninPromoLastAccountEmails();
        // Don't show if no new accounts have been added after the last time promo was shown.
        return previousAccountEmails == null
                || !previousAccountEmails.containsAll(currentAccountEmails);
    }

    private FullScreenSyncPromoUtil() {}
}
