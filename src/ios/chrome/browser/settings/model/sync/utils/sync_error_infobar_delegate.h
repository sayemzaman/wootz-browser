// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_ERROR_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_ERROR_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#import "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

class ChromeBrowserState;
@protocol SyncPresenter;

namespace infobars {
class InfoBarManager;
}

// Shows a sync error in an infobar.
class SyncErrorInfoBarDelegate : public ConfirmInfoBarDelegate,
                                 public syncer::SyncServiceObserver {
 public:
  SyncErrorInfoBarDelegate(ChromeBrowserState* browser_state,
                           id<SyncPresenter> presenter);

  SyncErrorInfoBarDelegate(const SyncErrorInfoBarDelegate&) = delete;
  SyncErrorInfoBarDelegate& operator=(const SyncErrorInfoBarDelegate&) = delete;

  ~SyncErrorInfoBarDelegate() override;

  // Creates a sync error infobar and adds it to `infobar_manager`.
  static bool Create(infobars::InfoBarManager* infobar_manager,
                     ChromeBrowserState* browser_state,
                     id<SyncPresenter> presenter);

  // InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  std::u16string GetTitleText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  raw_ptr<ChromeBrowserState> browser_state_;
  syncer::SyncService::UserActionableError error_state_;
  std::u16string title_;
  std::u16string message_;
  std::u16string button_text_;
  id<SyncPresenter> presenter_;
};

#endif  // IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_ERROR_INFOBAR_DELEGATE_H_
