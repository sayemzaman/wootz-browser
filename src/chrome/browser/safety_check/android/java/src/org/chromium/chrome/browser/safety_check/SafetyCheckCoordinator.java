// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge.usesSplitStoresAndUPMForLocal;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.Observer;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the Safety check settings page. */
public class SafetyCheckCoordinator implements DefaultLifecycleObserver, SafetyCheckComponentUi {
    private SafetyCheckSettingsFragment mSettingsFragment;
    private SafetyCheckUpdatesDelegate mUpdatesClient;
    private SyncConsentActivityLauncher mSigninLauncher;
    private SafetyCheckMediator mMediator;
    private SyncService mSyncService;
    private PrefService mPrefService;
    private PropertyModel mPasswordCheckLocalModel;
    private PropertyModel mPasswordCheckAccountModel;

    /**
     * Creates a new instance given a settings fragment, an updates client, and a settings launcher.
     * There is no need to hold on to a reference since the settings fragment's lifecycle is
     * observed and a reference is retained there.
     *
     * @param settingsFragment An instance of {@link SafetyCheckSettingsFragment} to observe.
     * @param updatesClient An instance implementing the {@link SafetyCheckUpdatesDelegate}
     *     interface.
     * @param bridge An instances of {@link SafetyCheckBridge} to access C++ APIs.
     * @param settingsLauncher An instance implementing the {@link SettingsLauncher} interface.
     * @param signinLauncher An instance implementing {@link SyncConsentActivityLauncher}.
     * @param modalDialogManagerSupplier An supplier for the {@link ModalDialogManager}.
     * @param passwordManagerHelper An instance of {@link PasswordManagerHelper} that provides
     *     access to password management capabilities.
     */
    public static void create(
            SafetyCheckSettingsFragment settingsFragment,
            SafetyCheckUpdatesDelegate updatesClient,
            SafetyCheckBridge bridge,
            SettingsLauncher settingsLauncher,
            SyncConsentActivityLauncher signinLauncher,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @Nullable SyncService syncService,
            PrefService prefService,
            PasswordManagerHelper passwordManagerHelper) {
        new SafetyCheckCoordinator(
                settingsFragment,
                updatesClient,
                bridge,
                settingsLauncher,
                signinLauncher,
                modalDialogManagerSupplier,
                syncService,
                prefService,
                passwordManagerHelper);
    }

    private SafetyCheckCoordinator(
            SafetyCheckSettingsFragment settingsFragment,
            SafetyCheckUpdatesDelegate updatesClient,
            SafetyCheckBridge bridge,
            SettingsLauncher settingsLauncher,
            SyncConsentActivityLauncher signinLauncher,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @Nullable SyncService syncService,
            PrefService prefService,
            PasswordManagerHelper passwordManagerHelper) {
        mSettingsFragment = settingsFragment;
        mUpdatesClient = updatesClient;
        mSyncService = syncService;
        mPrefService = prefService;
        mSettingsFragment.setComponentDelegate(this);
        // Create the model and the mediator once the view is created.
        // The view's lifecycle is not available at this point, so observe the {@link LiveData} for
        // it to get notified when {@link onCreateView} is called.
        mSettingsFragment
                .getViewLifecycleOwnerLiveData()
                .observe(
                        mSettingsFragment,
                        new Observer<LifecycleOwner>() {
                            @Override
                            public void onChanged(LifecycleOwner lifecycleOwner) {
                                // Only interested in the event when the View becomes non-null,
                                // which happens when {@link onCreateView} is invoked.
                                if (lifecycleOwner == null) {
                                    return;
                                }
                                // Only initialize it if it hasn't been already. This guards against
                                // multiple invocations of this method.
                                if (mMediator == null) {
                                    // Can start observing the View's lifecycle now.
                                    lifecycleOwner
                                            .getLifecycle()
                                            .addObserver(SafetyCheckCoordinator.this);
                                    // The View is available, so now we can create the Model, MCP,
                                    // and Mediator.
                                    PropertyModel safetyCheckModel =
                                            createSafetyCheckModelAndBind(mSettingsFragment);
                                    createPasswordCheckModels(mSettingsFragment, safetyCheckModel);
                                    mMediator =
                                            new SafetyCheckMediator(
                                                    safetyCheckModel,
                                                    mPasswordCheckAccountModel,
                                                    mPasswordCheckLocalModel,
                                                    mUpdatesClient,
                                                    bridge,
                                                    settingsLauncher,
                                                    signinLauncher,
                                                    syncService,
                                                    prefService,
                                                    passwordManagerHelper,
                                                    modalDialogManagerSupplier);
                                }
                            }
                        });
        // Show the initial state every time the fragment is resumed (navigation from a different
        // screen, app in the background, etc).
        mSettingsFragment
                .getLifecycle()
                .addObserver(
                        new DefaultLifecycleObserver() {
                            @Override
                            public void onResume(LifecycleOwner lifecycleOwner) {
                                if (mSettingsFragment.shouldRunSafetyCheckImmediately()) {
                                    mMediator.performSafetyCheck();
                                    return;
                                }
                                mMediator.setInitialState();
                            }
                        });
    }

    @VisibleForTesting
    static PropertyModel createSafetyCheckModelAndBind(
            SafetyCheckSettingsFragment settingsFragment) {
        PropertyModel model = SafetyCheckProperties.createSafetyCheckModel();
        PropertyModelChangeProcessor.create(model, settingsFragment, SafetyCheckViewBinder::bind);
        return model;
    }

    private void createPasswordCheckModels(
            SafetyCheckSettingsFragment settingsFragment, PropertyModel safetyCheckModel) {
        if (isAccountPasswordStorageUsed()) {
            String title =
                    usesSplitStoresAndUPMForLocal(mPrefService)
                            ? mSettingsFragment.getString(
                                    R.string.safety_check_passwords_account_title,
                                    CoreAccountInfo.getEmailFrom(mSyncService.getAccountInfo()))
                            : mSettingsFragment.getString(R.string.safety_check_passwords_title);
            mPasswordCheckAccountModel =
                    createPasswordCheckPreferenceModelAndBind(
                            settingsFragment,
                            safetyCheckModel,
                            SafetyCheckViewBinder.PASSWORDS_KEY_ACCOUNT,
                            title);
        }
        if (isLocalPasswordStorageUsed()) {
            String title =
                    usesSplitStoresAndUPMForLocal(mPrefService)
                            ? mSettingsFragment.getString(
                                    R.string.safety_check_passwords_local_title)
                            : mSettingsFragment.getString(R.string.safety_check_passwords_title);
            mPasswordCheckLocalModel =
                    createPasswordCheckPreferenceModelAndBind(
                            settingsFragment,
                            safetyCheckModel,
                            SafetyCheckViewBinder.PASSWORDS_KEY_LOCAL,
                            title);
        }
    }

    static PropertyModel createPasswordCheckPreferenceModelAndBind(
            SafetyCheckSettingsFragment settingsFragment,
            PropertyModel safetyCheckModel,
            String preferenceViewId,
            String preferenceTitle) {
        PropertyModel passwordSafetyCheckModel =
                PasswordsCheckPreferenceProperties.createPasswordSafetyCheckModel(preferenceTitle);
        PropertyModelChangeProcessor.create(
                passwordSafetyCheckModel,
                settingsFragment,
                (model, fragment, key) ->
                        SafetyCheckViewBinder.bindPasswordCheckPreferenceModel(
                                safetyCheckModel, model, fragment, key, preferenceViewId));
        return passwordSafetyCheckModel;
    }

    /** Gets invoked when the Fragment detaches (the View is destroyed ). */
    @Override
    public void onDestroy(LifecycleOwner owner) {
        // Stop observing the Lifecycle of the View as it is about to be destroyed.
        owner.getLifecycle().removeObserver(this);
        // Cancel any pending tasks.
        mMediator.destroy();
        // Clean up any objects we are holding on to.
        mSettingsFragment = null;
        mUpdatesClient = null;
        mMediator = null;
    }

    @Override
    public boolean isLocalPasswordStorageUsed() {
        if (!PasswordManagerHelper.hasChosenToSyncPasswords(mSyncService)) return true;
        if (usesSplitStoresAndUPMForLocal(mPrefService)) return true;
        return false;
    }

    @Override
    public boolean isAccountPasswordStorageUsed() {
        if (PasswordManagerHelper.hasChosenToSyncPasswords(mSyncService)) return true;
        return false;
    }
}
