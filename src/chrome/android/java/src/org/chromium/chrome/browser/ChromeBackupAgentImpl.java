// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.backup.BackupDataInput;
import android.app.backup.BackupDataOutput;
import android.app.backup.BackupManager;
import android.content.SharedPreferences;
import android.os.ParcelFileDescriptor;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.base.SplitCompatApplication;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.AsyncInitTaskRunner;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.sync.internal.SyncPrefNames;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.common.ContentProcessInfo;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Backup agent for Chrome, using Android key/value backup. */
@SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
public class ChromeBackupAgentImpl extends ChromeBackupAgent.Impl {
    private static final String ANDROID_DEFAULT_PREFIX = "AndroidDefault.";
    private static final String NATIVE_BOOL_PREF_PREFIX = "native.";
    private static final String NATIVE_DICT_PREF_PREFIX = "NativeJsonDict.";

    private static final String TAG = "ChromeBackupAgent";

    @VisibleForTesting
    static final String HISTOGRAM_ANDROID_RESTORE_RESULT = "Android.RestoreResult";

    // Restore status is used to pass the result of any restore to Chrome's first run, so that
    // it can be recorded as a histogram.
    @IntDef({
        RestoreStatus.NO_RESTORE,
        RestoreStatus.RESTORE_COMPLETED,
        RestoreStatus.RESTORE_AFTER_FIRST_RUN,
        RestoreStatus.BROWSER_STARTUP_FAILED,
        RestoreStatus.NOT_SIGNED_IN,
        RestoreStatus.DEPRECATED_SIGNIN_TIMED_OUT,
        RestoreStatus.DEPRECATED_RESTORE_STATUS_RECORDED,
        RestoreStatus.SIGNIN_TIMED_OUT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface RestoreStatus {
        // Values must match those in histogram.xml AndroidRestoreResult.
        int NO_RESTORE = 0;
        int RESTORE_COMPLETED = 1;
        int RESTORE_AFTER_FIRST_RUN = 2;
        int BROWSER_STARTUP_FAILED = 3;
        int NOT_SIGNED_IN = 4;
        // This enum value has taken the previous value indicating that the histogram has been
        // recorded, when it was introduced. Deprecating since the metric is polluted consequently.
        int DEPRECATED_SIGNIN_TIMED_OUT = 5;
        // Previously, DEPRECATED_RESTORE_STATUS_RECORDED was set when the histogram has been
        // recorded, to prevent additional histogram record. This magic value is being replaced by
        // the boolean pref RESTORE_STATUS_RECORDED.
        // This value is kept for legacy pref support.
        int DEPRECATED_RESTORE_STATUS_RECORDED = 6;
        int SIGNIN_TIMED_OUT = 7;

        int NUM_ENTRIES = 7;
    }

    @VisibleForTesting static final String RESTORE_STATUS = "android_restore_status";
    private static final String RESTORE_STATUS_RECORDED = "android_restore_status_recorded";

    // Keep track of backup failures, so that we give up in the end on persistent problems.
    @VisibleForTesting static final String BACKUP_FAILURE_COUNT = "android_backup_failure_count";
    @VisibleForTesting static final int MAX_BACKUP_FAILURES = 5;

    // Bool entries from SharedPreferences that should be backed up / restored.
    static final String[] BACKUP_ANDROID_BOOL_PREFS = {
        ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED,
        ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE,
        ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE,
        ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY,
        ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER,
    };

    // Bool entries from PrefService that should be backed up / restored.
    static final String[] BACKUP_NATIVE_SYNC_TYPE_BOOL_PREFS = {
        SyncPrefNames.SYNC_KEEP_EVERYTHING_SYNCED,
        SyncPrefNames.SYNC_APPS,
        SyncPrefNames.SYNC_AUTOFILL,
        SyncPrefNames.SYNC_BOOKMARKS,
        SyncPrefNames.SYNC_COMPARE,
        SyncPrefNames.SYNC_HISTORY,
        SyncPrefNames.SYNC_PASSWORDS,
        SyncPrefNames.SYNC_PAYMENTS,
        SyncPrefNames.SYNC_PREFERENCES,
        SyncPrefNames.SYNC_READING_LIST,
        SyncPrefNames.SYNC_SAVED_TAB_GROUPS,
        SyncPrefNames.SYNC_SHARED_TAB_GROUP_DATA,
        SyncPrefNames.SYNC_TABS,
    };

    // Key used to store the email of the syncing account. This email is obtained from
    // IdentityManager during the backup.
    static final String SYNCING_ACCOUNT_KEY = "google.services.username";

    // Key used to store the email of the signed-in account. This email is obtained from
    // IdentityManager during the backup.
    static final String SIGNED_IN_ACCOUNT_ID_KEY = "Chrome.SignIn.SignedInAccountGaiaIdBackup";

    // Timeout for running the background tasks, needs to be quite long since they may be doing
    // network access, but must be less than the 1 minute restore timeout to be useful.
    private static final long BACKGROUND_TASK_TIMEOUT_SECS = 20;

    // Timeout for the sign-in flow and related preferences commit.
    private static final long SIGNIN_TIMEOUT_SECS = 10;

    /**
     * Class to save and restore the backup state, used to decide if backups are needed. Since the
     * backup data is small, and stored as private data by the backup service, this can simply store
     * and compare a copy of the data.
     */
    private static final class BackupState {
        private ArrayList<String> mNames;
        private ArrayList<byte[]> mValues;

        @SuppressWarnings("unchecked")
        public BackupState(ParcelFileDescriptor parceledState) throws IOException {
            if (parceledState == null) return;
            try {
                FileInputStream instream = new FileInputStream(parceledState.getFileDescriptor());
                ObjectInputStream in = new ObjectInputStream(instream);
                mNames = (ArrayList<String>) in.readObject();
                mValues = (ArrayList<byte[]>) in.readObject();
            } catch (ClassNotFoundException e) {
                throw new RuntimeException(e);
            }
        }

        public BackupState(ArrayList<String> names, ArrayList<byte[]> values) {
            mNames = names;
            mValues = values;
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof BackupState)) return false;
            BackupState otherBackupState = (BackupState) other;
            return mNames.equals(otherBackupState.mNames)
                    && Arrays.deepEquals(mValues.toArray(), otherBackupState.mValues.toArray());
        }

        public void save(ParcelFileDescriptor parceledState) throws IOException {
            FileOutputStream outstream = new FileOutputStream(parceledState.getFileDescriptor());
            ObjectOutputStream out = new ObjectOutputStream(outstream);
            out.writeObject(mNames);
            out.writeObject(mValues);
        }
    }

    // TODO (aberent) Refactor the tests to use a mocked ChromeBrowserInitializer, and make this
    // private again.
    @VisibleForTesting
    boolean initializeBrowser() {
        // Workaround for https://crbug.com/718166. The backup agent is sometimes being started in a
        // child process, before the child process loads its native library. If backup then loads
        // the native library the child process is left in a very confused state and crashes.
        if (ContentProcessInfo.inChildProcess()) {
            Log.e(TAG, "Backup agent started from child process");
            return false;
        }
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        return true;
    }

    private static byte[] booleanToBytes(boolean value) {
        return value ? new byte[] {1} : new byte[] {0};
    }

    private static boolean bytesToBoolean(byte[] bytes) {
        return bytes[0] != 0;
    }

    @Override
    public void onBackup(
            ParcelFileDescriptor oldState, BackupDataOutput data, ParcelFileDescriptor newState)
            throws IOException {
        final ArrayList<String> backupNames = new ArrayList<>();
        final ArrayList<byte[]> backupValues = new ArrayList<>();

        // TODO(crbug.com/40066949): Remove syncAccount once UNO is launched, given the sync feature
        // and consent will disappear.
        final AtomicReference<CoreAccountInfo> syncAccount = new AtomicReference<>();
        final AtomicReference<CoreAccountInfo> signedInAccount = new AtomicReference<>();

        // The native preferences can only be read on the UI thread.
        Boolean nativePrefsRead =
                PostTask.runSynchronously(
                        TaskTraits.UI_DEFAULT,
                        () -> {
                            // Start the browser if necessary, so that Chrome can access the native
                            // preferences. Although Chrome requests the backup, it doesn't happen
                            // immediately, so by the time it does Chrome may not be running.
                            if (!initializeBrowser()) return false;

                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            IdentityManager identityManager =
                                    IdentityServicesProvider.get().getIdentityManager(profile);
                            syncAccount.set(
                                    identityManager.getPrimaryAccountInfo(ConsentLevel.SYNC));
                            signedInAccount.set(
                                    identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN));

                            if (syncAccount.get() != null
                                    && !syncAccount.get().equals(signedInAccount.get())) {
                                throw new IllegalStateException(
                                        "Recorded signed in account differs from syncing account");
                            }

                            // When new data type is added to the UserSelectableType enum, also add
                            // it to BACKUP_NATIVE_SYNC_TYPE_BOOL_PREFS (if the type is supported on
                            // Android).
                            assert UserSelectableType.LAST_TYPE == 14;
                            PrefService prefService = UserPrefs.get(profile);
                            for (String name : BACKUP_NATIVE_SYNC_TYPE_BOOL_PREFS) {
                                backupNames.add(NATIVE_BOOL_PREF_PREFIX + name);
                                backupValues.add(booleanToBytes(prefService.getBoolean(name)));
                            }
                            backupNames.add(
                                    NATIVE_DICT_PREF_PREFIX
                                            + SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT);
                            backupValues.add(
                                    ChromeBackupAgentImplJni.get()
                                            .getSerializedDict(
                                                    prefService,
                                                    SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT)
                                            .getBytes());

                            return true;
                        });
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();

        if (!nativePrefsRead) {
            // Something went wrong reading the native preferences, skip the backup, but try again
            // later.
            int backupFailureCount = sharedPrefs.getInt(BACKUP_FAILURE_COUNT, 0) + 1;
            if (backupFailureCount >= MAX_BACKUP_FAILURES) {
                // Too many re-tries, give up and force an unconditional backup next time one is
                // requested.
                return;
            }
            sharedPrefs.edit().putInt(BACKUP_FAILURE_COUNT, backupFailureCount).apply();
            if (oldState != null) {
                try {
                    // Copy the old state to the new state, so that next time Chrome only does a
                    // backup if necessary.
                    BackupState state = new BackupState(oldState);
                    state.save(newState);
                } catch (Exception e) {
                    // There was no old state, or it was corrupt; leave the newState unwritten,
                    // hence forcing an unconditional backup on the next attempt.
                }
            }
            // Ask Android to schedule a retry.
            new BackupManager(getBackupAgent()).dataChanged();
            return;
        }

        // The backup is going to work, clear the failure count.
        sharedPrefs.edit().remove(BACKUP_FAILURE_COUNT).apply();

        // Add the Android boolean prefs.
        for (String prefName : BACKUP_ANDROID_BOOL_PREFS) {
            if (sharedPrefs.contains(prefName)) {
                backupNames.add(ANDROID_DEFAULT_PREFIX + prefName);
                backupValues.add(booleanToBytes(sharedPrefs.getBoolean(prefName, false)));
            }
        }

        // Finally add the signed-in/syncing user ids.
        backupNames.add(ANDROID_DEFAULT_PREFIX + SYNCING_ACCOUNT_KEY);
        backupValues.add(
                ApiCompatibilityUtils.getBytesUtf8(
                        syncAccount.get() == null ? "" : syncAccount.get().getEmail()));
        backupNames.add(ANDROID_DEFAULT_PREFIX + SIGNED_IN_ACCOUNT_ID_KEY);
        backupValues.add(
                ApiCompatibilityUtils.getBytesUtf8(
                        signedInAccount.get() == null ? "" : signedInAccount.get().getGaiaId()));

        BackupState newBackupState = new BackupState(backupNames, backupValues);

        // Check if a backup is actually needed.
        try {
            BackupState oldBackupState = new BackupState(oldState);
            if (newBackupState.equals(oldBackupState)) {
                Log.i(TAG, "Nothing has changed since the last backup. Backup skipped.");
                newBackupState.save(newState);
                return;
            }
        } catch (IOException e) {
            // This will happen if Chrome has never written backup data, or if the backup status is
            // corrupt. Create a new backup in either case.
            Log.i(TAG, "Can't read backup status file");
        }

        // Write the backup data
        for (int i = 0; i < backupNames.size(); i++) {
            data.writeEntityHeader(backupNames.get(i), backupValues.get(i).length);
            data.writeEntityData(backupValues.get(i), backupValues.get(i).length);
        }

        // Remember the backup state.
        newBackupState.save(newState);

        Log.i(TAG, "Backup complete");
    }

    @Override
    public void onRestore(BackupDataInput data, int appVersionCode, ParcelFileDescriptor newState)
            throws IOException {
        // TODO(aberent) Check that this is not running on the UI thread. Doing so, however, makes
        // testing difficult since the test code runs on the UI thread.

        // Check that the user hasn't already seen FRE (not sure if this can ever happen, but if it
        // does then restoring the backup will overwrite the user's choices).
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        if (FirstRunStatus.getFirstRunFlowComplete()
                || FirstRunStatus.getLightweightFirstRunFlowComplete()) {
            setRestoreStatus(RestoreStatus.RESTORE_AFTER_FIRST_RUN);
            Log.w(TAG, "Restore attempted after first run");
            return;
        }

        final ArrayList<String> backupNames = new ArrayList<>();
        final ArrayList<byte[]> backupValues = new ArrayList<>();

        @Nullable String restoredSyncUserEmail = null;
        @Nullable String restoredSignedInUserID = null;
        while (data.readNextHeader()) {
            String key = data.getKey();
            int dataSize = data.getDataSize();
            byte[] buffer = new byte[dataSize];
            data.readEntityData(buffer, 0, dataSize);
            if (key.equals(ANDROID_DEFAULT_PREFIX + SYNCING_ACCOUNT_KEY)) {
                restoredSyncUserEmail = new String(buffer);
            } else if (key.equals(ANDROID_DEFAULT_PREFIX + SIGNED_IN_ACCOUNT_ID_KEY)) {
                restoredSignedInUserID = new String(buffer);
            } else {
                backupNames.add(key);
                backupValues.add(buffer);
            }
        }

        // Start and wait for the Async init tasks. This loads the library, and attempts to load the
        // first run variations seed. Since these are both slow it makes sense to run them in
        // parallel as Android AsyncTasks, reusing some of Chrome's async startup logic.
        //
        // Note that this depends on onRestore being run from a background thread, since
        // if it were called from the UI thread the broadcast would not be received until after it
        // exited.
        final CountDownLatch latch = new CountDownLatch(1);
        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    // TODO(crbug.com/40283943): Wait for AccountManagerFacade to load accounts.
                    // Chrome library loading depends on PathUtils.
                    PathUtils.setPrivateDataDirectorySuffix(
                            SplitCompatApplication.PRIVATE_DATA_DIRECTORY_SUFFIX);
                    createAsyncInitTaskRunner(latch)
                            .startBackgroundTasks(
                                    /* allocateChildConnection= */ false,
                                    /* fetchVariationSeed= */ true);
                });

        try {
            // Ignore result. It will only be false if it times out. Problems with fetching the
            // variation seed can be ignored, and other problems will either recover or be repeated
            // when Chrome is started synchronously.
            latch.await(BACKGROUND_TASK_TIMEOUT_SECS, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            // Should never happen, but can be ignored (as explained above) anyway.
        }

        // Chrome has to be running before it can check if the account exists. Because the native
        // library is already loaded Chrome startup should be fast.
        boolean browserStarted =
                PostTask.runSynchronously(
                        TaskTraits.UI_DEFAULT,
                        () -> {
                            // Start the browser if necessary.
                            return initializeBrowser();
                        });
        if (!browserStarted) {
            // Something went wrong starting Chrome, skip the restore.
            setRestoreStatus(RestoreStatus.BROWSER_STARTUP_FAILED);
            return;
        }

        @Nullable
        CoreAccountInfo signedInAccountInfo = getDeviceAccountWithGaiaId(restoredSignedInUserID);
        @Nullable
        CoreAccountInfo syncAccountInfo = getDeviceAccountWithEmail(restoredSyncUserEmail);

        // If the user hasn't signed in, or can't sign in, then don't restore anything.
        if (syncAccountInfo == null
                && (signedInAccountInfo == null
                        || !SigninFeatureMap.isEnabled(
                                SigninFeatures
                                        .RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP))) {
            setRestoreStatus(RestoreStatus.NOT_SIGNED_IN);
            Log.i(TAG, "Chrome was not signed in with a known account name, not restoring");
            return;
        }

        // Restore the native preferences on the UI thread
        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    boolean areAccountSettingsRestored = false;

                    for (int i = 0; i < backupNames.size(); i++) {
                        String name = backupNames.get(i);
                        if (name.startsWith(NATIVE_BOOL_PREF_PREFIX)) {
                            name = name.substring(NATIVE_BOOL_PREF_PREFIX.length());
                            if (!Arrays.asList(BACKUP_NATIVE_SYNC_TYPE_BOOL_PREFS).contains(name)) {
                                // Not among the known prefs, do not restore. In the worst case,
                                // this could attempt to write a pref which is no longer exists,
                                // causing a crash.
                                continue;
                            }

                            prefService.setBoolean(name, bytesToBoolean(backupValues.get(i)));
                            continue;
                        }

                        // Restore the account settings if possible.
                        // It should be done before the potential migration of global boolean
                        // preferences to account settings:
                        // - If the user was syncing, the global prefs are more up-to-date so the
                        // converted global prefs should take precedence;
                        // - If the user was signed-in only, the global preferences will not be
                        // migrated to account settings if the latter is restored, so no risk of
                        // override here.
                        if (name.startsWith(NATIVE_DICT_PREF_PREFIX)) {
                            name = name.substring(NATIVE_DICT_PREF_PREFIX.length());
                            if (!name.equals(SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT)
                                    || !SigninFeatureMap.isEnabled(
                                            SigninFeatures
                                                    .RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP)) {
                                // Same as above, do not restore prefs if the name is unknown
                                // or if the restore flag is not enabled.
                                continue;
                            }

                            areAccountSettingsRestored = true;
                            ChromeBackupAgentImplJni.get()
                                    .setDict(
                                            prefService,
                                            SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT,
                                            new String(backupValues.get(i)));
                            continue;
                        }
                    }

                    // Migrate global sync settings to account settings when necessary.
                    // It should be done after the restoration of the existing per-account settings
                    // from the backup to avoid override, as mentioned above.
                    final boolean shouldRestoreSelectedTypesAsAccountSettings =
                            (syncAccountInfo != null || !areAccountSettingsRestored)
                                    && SigninFeatureMap.isEnabled(
                                            SigninFeatures
                                                    .RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP)
                                    && ChromeFeatureList.isEnabled(
                                            ChromeFeatureList
                                                    .REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);
                    if (shouldRestoreSelectedTypesAsAccountSettings) {
                        final String gaiaID =
                                syncAccountInfo != null
                                        ? syncAccountInfo.getGaiaId()
                                        : signedInAccountInfo.getGaiaId();
                        ChromeBackupAgentImplJni.get()
                                .migrateGlobalDataTypePrefsToAccount(prefService, gaiaID);
                    }

                    // TODO(crbug.com/332710541): Another commit is done for signed-in users in
                    // SigninManager.SignInCallback.onPrefsCommitted(). Do a single one instead.
                    ChromeBackupAgentImplJni.get().commitPendingPrefWrites(prefService);
                });

        // Now that everything looks good so restore the Android preferences.
        SharedPreferences.Editor editor = sharedPrefs.edit();

        // Only restore preferences that we know about.
        int prefixLength = ANDROID_DEFAULT_PREFIX.length();
        for (int i = 0; i < backupNames.size(); i++) {
            String name = backupNames.get(i);
            if (name.startsWith(ANDROID_DEFAULT_PREFIX)
                    && Arrays.asList(BACKUP_ANDROID_BOOL_PREFS)
                            .contains(name.substring(prefixLength))) {
                editor.putBoolean(
                        name.substring(prefixLength), bytesToBoolean(backupValues.get(i)));
            }
        }

        // TODO(crbug.com/40075135): Restore the metrics related preferences and update the upload
        // states as early as possible, i.e. before the native prefs restoration/getting accounts.
        // Refresh the metrics service state after related preferences are restored, to allow the
        // experiment metrics to be sent if there's any.
        if (SigninFeatureMap.isEnabled(SigninFeatures.UPDATE_METRICS_SERVICES_STATE_IN_RESTORE)) {
            UmaSessionStats.updateMetricsServiceState();
        }

        if (syncAccountInfo != null) {
            // Both accounts are recorded at the same time. Since only one account is in signed-in
            // state at a given time, they should be identical if both are valid.
            if (signedInAccountInfo != null && !signedInAccountInfo.equals(syncAccountInfo)) {
                throw new IllegalStateException(
                        "Recorded signed in account differs from syncing account");
            }

            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
                editor.apply();
                signInAndWaitForResult(syncAccountInfo);
            } else {
                // This will sign in the user on first run to the account in
                // BACKUP_FLOW_SIGNIN_ACCOUNT_NAME if any.
                editor.putString(
                        ChromePreferenceKeys.BACKUP_FLOW_SIGNIN_ACCOUNT_NAME,
                        restoredSyncUserEmail);
                editor.apply();

                // The silent first run will change things, so there is no point in trying to
                // prevent
                // additional backups at this stage. Don't write anything to |newState|.
                setRestoreStatus(RestoreStatus.RESTORE_COMPLETED);
            }
        } else {
            editor.apply();

            // signedInAccountInfo and syncAccountInfo should not be null at the same at this point.
            // If there's no valid syncing account and the signed-in account restore is disabled,
            // the restore should already be stopped and the restore state set to `NOT_SIGNED_IN`.
            if (signedInAccountInfo == null
                    || !SigninFeatureMap.isEnabled(
                            SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP)) {
                throw new IllegalStateException("No valid account can be signed-in");
            }

            signInAndWaitForResult(signedInAccountInfo);
        }
        Log.i(TAG, "Restore complete");
    }

    @VisibleForTesting
    AsyncInitTaskRunner createAsyncInitTaskRunner(final CountDownLatch latch) {
        return new AsyncInitTaskRunner() {
            @Override
            protected void onSuccess() {
                latch.countDown();
            }

            @Override
            protected void onFailure(Exception failureCause) {
                // Ignore failure. Problems with the variation seed can be ignored, and other
                // problems will either recover or be repeated when Chrome is started synchronously.
                latch.countDown();
            }
        };
    }

    private @Nullable CoreAccountInfo getDeviceAccountWithEmail(@Nullable String accountEmail) {
        if (accountEmail == null) {
            return null;
        }

        return PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    return AccountUtils.findCoreAccountInfoByEmail(getAccountInfos(), accountEmail);
                });
    }

    private @Nullable CoreAccountInfo getDeviceAccountWithGaiaId(@Nullable String accountGaiaId) {
        if (accountGaiaId == null) {
            return null;
        }

        return PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    return AccountUtils.findCoreAccountInfoByGaiaId(
                            getAccountInfos(), accountGaiaId);
                });
    }

    private static List<CoreAccountInfo> getAccountInfos() {
        return AccountManagerFacadeProvider.getInstance().getCoreAccountInfos().getResult();
    }

    private static void signInAndWaitForResult(CoreAccountInfo accountInfo) {
        final CountDownLatch latch = new CountDownLatch(1);
        SigninManager.SignInCallback signInCallback =
                new SigninManager.SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        // Sign-in preferences need to be committed for the sign-in to be effective.
                        // Therefore the count down is done in `onPrefsCommitted` instead.
                    }

                    @Override
                    public void onPrefsCommitted() {
                        latch.countDown();
                    }

                    @Override
                    public void onSignInAborted() {
                        // Ignore failure as Chrome will simply remain signed-out otherwise, and the
                        // user is still able to sign-in manually after opening Chrome.
                        latch.countDown();
                    }
                };

        signIn(accountInfo, signInCallback);

        try {
            // Wait the sign-in to finish the restore. Otherwise, the account info request will be
            // cancelled one the restore ends. Timeout can be ignored as Chrome will simply remain
            // signed-out otherwise, and the user is still able to sign-in manually after opening
            // Chrome.
            boolean success = latch.await(SIGNIN_TIMEOUT_SECS, TimeUnit.SECONDS);
            int status = success ? RestoreStatus.RESTORE_COMPLETED : RestoreStatus.SIGNIN_TIMED_OUT;
            setRestoreStatus(status);
        } catch (InterruptedException e) {
            // Exception can be ignored as explained above.
            setRestoreStatus(RestoreStatus.SIGNIN_TIMED_OUT);
        }
    }

    private static void signIn(CoreAccountInfo accountInfo, SigninManager.SignInCallback callback) {
        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    SigninManager signinManager =
                            IdentityServicesProvider.get()
                                    .getSigninManager(ProfileManager.getLastUsedRegularProfile());
                    final AccountManagerFacade accountManagerFacade =
                            AccountManagerFacadeProvider.getInstance();

                    Runnable signinRunnable =
                            () -> {
                                signinManager.runAfterOperationInProgress(
                                        () -> {
                                            signinManager.signin(
                                                    accountInfo,
                                                    SigninAccessPoint
                                                            .POST_DEVICE_RESTORE_BACKGROUND_SIGNIN,
                                                    callback);
                                        });
                            };

                    Callback<Boolean> accountManagedCallback =
                            (isManaged) -> {
                                // If restoring a managed account, the user most likely already
                                // accepted account management previously and we don't have the
                                // ability to re-show the confirmation dialog here anyways.
                                if (isManaged) signinManager.setUserAcceptedAccountManagement(true);
                                signinRunnable.run();
                            };

                    AccountManagerFacade.ChildAccountStatusListener listener =
                            (isChild, unused) -> {
                                if (isChild) {
                                    // TODO(crbug.com/40835324):
                                    // Pre-AllowSyncOffForChildAccounts, the backup sign-in for
                                    // child accounts would happen in SigninChecker anyways.
                                    // Maybe it should be handled by this  class once the
                                    // feature launches.
                                    callback.onSignInAborted();
                                    return;
                                }

                                if (SigninFeatureMap.isEnabled(
                                        SigninFeatures.ENTERPRISE_POLICY_ON_SIGNIN)) {
                                    signinManager.isAccountManaged(
                                            accountInfo, accountManagedCallback);
                                } else {
                                    signinRunnable.run();
                                }
                            };

                    AccountUtils.checkChildAccountStatus(
                            accountManagerFacade, getAccountInfos(), listener);
                });
    }

    /**
     * Get the saved result of any restore that may have happened.
     *
     * @return the restore status, a RestoreStatus value.
     */
    @VisibleForTesting
    static @RestoreStatus int getRestoreStatus() {
        return ContextUtils.getAppSharedPreferences()
                .getInt(RESTORE_STATUS, RestoreStatus.NO_RESTORE);
    }

    /**
     * Save the restore status for later transfer to a histogram, and reset histogram recorded
     * status if needed.
     *
     * @param status the status.
     */
    @VisibleForTesting
    static void setRestoreStatus(@RestoreStatus int status) {
        assert status != RestoreStatus.DEPRECATED_RESTORE_STATUS_RECORDED
                && status != RestoreStatus.DEPRECATED_SIGNIN_TIMED_OUT;

        ContextUtils.getAppSharedPreferences().edit().putInt(RESTORE_STATUS, status).apply();
        if (isRestoreStatusRecorded()) {
            setRestoreStatusRecorded(false);
        }
    }

    /**
     * Get from the saved values whether the restore status histogram has been recorded.
     *
     * @return Whether the restore status has been recorded.
     */
    @VisibleForTesting
    static boolean isRestoreStatusRecorded() {
        return ContextUtils.getAppSharedPreferences().getBoolean(RESTORE_STATUS_RECORDED, false);
    }

    /**
     * Save the value indicating whether the restore status histogram has been recorded.
     *
     * @param isRecorded Whether the restore status is recorded.
     */
    @VisibleForTesting
    static void setRestoreStatusRecorded(boolean isRecorded) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(RESTORE_STATUS_RECORDED, isRecorded)
                .apply();
    }

    /** Record the restore histogram. To be called from Chrome itself once it is running. */
    public static void recordRestoreHistogram() {
        boolean isStatusRecorded = isRestoreStatusRecorded();
        // Ensure restore status is only recorded once.
        if (isStatusRecorded) {
            return;
        }

        @RestoreStatus int restoreStatus = getRestoreStatus();
        if (restoreStatus != RestoreStatus.DEPRECATED_RESTORE_STATUS_RECORDED
                && restoreStatus != RestoreStatus.DEPRECATED_SIGNIN_TIMED_OUT) {
            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_ANDROID_RESTORE_RESULT, restoreStatus, RestoreStatus.NUM_ENTRIES);
        }
        setRestoreStatusRecorded(true);
    }

    @NativeMethods
    interface Natives {
        // See PrefService::CommitPendingWrite().
        void commitPendingPrefWrites(PrefService prefService);

        // Returns a serialized version of PrefService::GetDict(), which can be stored in backups.
        @JniType("std::string")
        String getSerializedDict(PrefService prefService, @JniType("std::string") String prefName);

        // If `serializedDict` was obtained from `getSerializedDict(prefService, prefName)`,
        // deserializes and passes the result to PrefService::SetDict(). If deserialization fails,
        // does nothing.
        void setDict(
                PrefService prefService,
                @JniType("std::string") String prefName,
                @JniType("std::string") String serializedDict);

        // Calls syncer::MigrateGlobalDataTypePrefsToAccount() to migrate global boolean sync prefs
        // to account settings.
        void migrateGlobalDataTypePrefsToAccount(
                PrefService prefService, @JniType("std::string") String gaiaId);
    }
}
