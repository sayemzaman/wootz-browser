// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_SERVICE_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_SERVICE_H_

#include <memory>
#include <optional>

#include "ash/public/cpp/accelerators.h"
#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/crosapi/full_restore_ash.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/session_types.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace app_restore {
class RestoreData;
}  // namespace app_restore

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

struct PineContentsData;

namespace full_restore {

class FullRestoreAppLaunchHandler;
class FullRestoreDataHandler;
class NewUserRestorePrefHandler;

extern const char kRestoreForCrashNotificationId[];
extern const char kRestoreNotificationId[];
extern const char kSetRestorePrefNotificationId[];

// The restore notification button index.
enum class RestoreNotificationButtonIndex {
  kRestore = 0,
  kCancel,
};

// This is used to record histograms, so do not remove or reorder existing
// entries.
enum class RestoreAction {
  kRestore = 0,
  kCancel = 1,
  kCloseByUser = 2,
  kCloseNotByUser = 3,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kCloseNotByUser,
};

// Returns true if FullRestoreService can be created to restore/launch Lacros
// during the system startup phase when all of the below conditions are met:
// 1. The FullRestoreForLacros flag is enabled.
// 2. Lacros is enabled.
// 3. FullRestoreService can be created for the primary profile.
bool MaybeCreateFullRestoreServiceForLacros();

// The FullRestoreService class calls AppService and Window Management
// interfaces to restore the app launchings and app windows.
class FullRestoreService : public KeyedService,
                           public message_center::NotificationObserver,
                           public AcceleratorController::Observer {
 public:
  // Delegate class that talks to ash shell. Ash shell is not created in
  // unit tests so this should be mocked out for testing those behaviors.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Starts overview with the pine dialog unless overview is already active.
    virtual void MaybeStartPineOverviewSession(
        std::unique_ptr<PineContentsData> pine_contents_data) = 0;
    virtual void MaybeEndPineOverviewSession() = 0;
  };

  static FullRestoreService* GetForProfile(Profile* profile);
  static void MaybeCloseNotification(Profile* profile);

  explicit FullRestoreService(Profile* profile);
  FullRestoreService(const FullRestoreService&) = delete;
  FullRestoreService& operator=(const FullRestoreService&) = delete;
  ~FullRestoreService() override;

  FullRestoreAppLaunchHandler* app_launch_handler() {
    return app_launch_handler_.get();
  }

  // Initialize the full restore service. |show_notification| indicates whether
  // a full restore notification has been shown.
  void Init(bool& show_notification);

  void OnTransitionedToNewActiveUser(Profile* profile);

  // Launches the browser, When the restore data is loaded, and the user chooses
  // to restore.
  void LaunchBrowserWhenReady();

  void MaybeCloseNotification(bool allow_save = true);

  // Implement the restoration.
  void Restore();

  // message_center::NotificationObserver:
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

  // AcceleratorController::Observer:
  void OnActionPerformed(AcceleratorAction action) override;
  void OnAcceleratorControllerWillBeDestroyed(
      AcceleratorController* controller) override;

  void SetAppLaunchHandlerForTesting(
      std::unique_ptr<FullRestoreAppLaunchHandler> app_launch_handler);

 private:
  friend class FullRestoreTestHelper;
  FRIEND_TEST_ALL_PREFIXES(FullRestoreAppLaunchHandlerChromeAppBrowserTest,
                           RestoreChromeApp);
  FRIEND_TEST_ALL_PREFIXES(FullRestoreAppLaunchHandlerArcAppBrowserTest,
                           RestoreArcApp);
  using SessionWindows = std::vector<std::unique_ptr<sessions::SessionWindow>>;
  // Maps window id to an associated session window. We use a map at certain
  // points because:
  //   - The data from the full restore file is in a 2 dimensional vector. The
  //     first one is for apps, and the second one is for windows.
  //   - The data from session restore is a single vector.
  // We build a map to avoid doing a O(n) search each loop of the former.
  using SessionWindowsMap =
      base::flat_map<int, crosapi::mojom::SessionWindowPtr>;

  // KeyedService:
  void Shutdown() override;

  // Returns true if `Init` can be called to show the notification or restore
  // apps. Otherwise, returns false.
  bool CanBeInited() const;

  // Show the restore notification on startup.
  void MaybeShowRestoreNotification(const std::string& id,
                                    bool& show_notification);

  void RecordRestoreAction(const std::string& notification_id,
                           RestoreAction restore_action);

  // Callback used when the pref |kRestoreAppsAndPagesPrefName| changes.
  void OnPreferenceChanged(const std::string& pref_name);

  void OnAppTerminating();

  // Callbacks for the pine dialog buttons.
  void RestoreForForest();
  void CancelForForest();

  // Callbacks run after querying for data from the session service(s).
  // `OnGotSessionAsh` is run after receiving data from either the normal
  // session service or app session service. `OnGotAllSessionsAsh` is run after
  // receiving data from both.
  void OnGotSessionAsh(base::OnceCallback<void(SessionWindows)> callback,
                       SessionWindows session_windows,
                       SessionID active_window_id,
                       bool read_error);
  void OnGotAllSessionsAsh(
      bool last_session_crashed,
      const std::vector<SessionWindows>& all_session_windows);
  void OnGotAllSessionsLacros(
      bool last_session_crashed,
      std::vector<crosapi::mojom::SessionWindowPtr> all_session_windows);

  // Called when session information is ready to be processed. Constructs the
  // object needed to show the pine dialog. It will be passed to ash which will
  // then use its contents to create and display the pine dialog. `restore_data`
  // is the data read from the full restore file. `session_windows_map` is the
  // browser info retrieved from session restore.
  void OnSessionInformationReceived(
      ::app_restore::RestoreData* restore_data,
      const SessionWindowsMap& session_windows_map,
      bool last_session_crashed);

  // Starts pine onboarding dialog when there is no restore data.
  void MaybeShowPineOnboarding();

  raw_ptr<Profile> profile_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;

  // If the user of `profile_` is not the primary user, and hasn't been the
  // active user yet, don't init to restore. Because if the restore setting is
  // 'Always', the app could be launched directly after restart, and the app
  // windows could be added to the primary user's profile path. This may cause
  // the non-primary user lost some restored windows data.
  //
  // If the non primary user becomes the active user, set `can_be_inited_` as
  // true to init and restore app. Otherwise, if `can_be_inited_` is false for
  // the non primary user, defer the init and app restoration.
  bool can_be_inited_ = false;

  bool is_shut_down_ = false;
  bool close_notification_ = false;

  // Specifies whether it is the first time to run the full restore feature.
  bool first_run_full_restore_ = false;

  // If the user clicks a notification button, set
  // |skip_notification_histogram_| as true to skip the notification close
  // histogram.
  bool skip_notification_histogram_ = false;

  std::unique_ptr<NewUserRestorePrefHandler> new_user_pref_handler_;

  // |app_launch_handler_| is responsible for launching apps based on the
  // restore data.
  std::unique_ptr<FullRestoreAppLaunchHandler> app_launch_handler_;

  std::unique_ptr<FullRestoreDataHandler> restore_data_handler_;

  std::unique_ptr<message_center::Notification> notification_;

  std::unique_ptr<Delegate> delegate_;

  base::CallbackListSubscription on_app_terminating_subscription_;

  // Browser session restore exit type service lock. This is created when the
  // system is restored from crash to help set the browser saving flag.
  std::unique_ptr<ExitTypeService::CrashedLock> crashed_lock_;

  base::ScopedObservation<AcceleratorController,
                          AcceleratorController::Observer>
      accelerator_controller_observer_{this};

  base::WeakPtrFactory<FullRestoreService> weak_ptr_factory_{this};
};

class ScopedRestoreForTesting {
 public:
  ScopedRestoreForTesting();
  ScopedRestoreForTesting(const ScopedRestoreForTesting&) = delete;
  ScopedRestoreForTesting& operator=(const ScopedRestoreForTesting&) = delete;
  ~ScopedRestoreForTesting();
};

}  // namespace full_restore

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_SERVICE_H_
