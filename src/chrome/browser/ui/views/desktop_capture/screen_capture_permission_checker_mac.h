// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SCREEN_CAPTURE_PERMISSION_CHECKER_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SCREEN_CAPTURE_PERMISSION_CHECKER_MAC_H_

#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/desktop_capture/screen_capture_permission_checker.h"

class ScreenCapturePermissionCheckerMac
    : public ScreenCapturePermissionChecker {
 public:
  // Create a ScreenCapturePermissionCheckerMac if it is enabled.
  static std::unique_ptr<ScreenCapturePermissionCheckerMac> MaybeCreate(
      base::RepeatingCallback<void(bool)> callback);

  explicit ScreenCapturePermissionCheckerMac(
      base::RepeatingCallback<void(bool)> callback);
  ~ScreenCapturePermissionCheckerMac() override;

  void Stop() override;

 private:
  void OnRecurrentPermissionCheck();
  void OnPermissionUpdate(bool has_permission);

  bool has_pending_task_ = false;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({});
  base::RepeatingCallback<void(bool)> callback_;
  base::RepeatingTimer timer_;
  base::WeakPtrFactory<ScreenCapturePermissionCheckerMac> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_SCREEN_CAPTURE_PERMISSION_CHECKER_MAC_H_
