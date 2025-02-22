// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_SYNCED_BOOKMARKS_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_SYNCED_BOOKMARKS_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"

class ChromeBrowserState;

namespace signin {
class IdentityManager;
}

namespace sync_bookmarks {

// Bridge class that will notify the panel when the remote bookmarks content
// change.
class SyncedBookmarksObserverBridge : public SyncObserverBridge {
 public:
  SyncedBookmarksObserverBridge(id<SyncObserverModelBridge> delegate,
                                ChromeBrowserState* browserState);

  SyncedBookmarksObserverBridge(const SyncedBookmarksObserverBridge&) = delete;
  SyncedBookmarksObserverBridge& operator=(
      const SyncedBookmarksObserverBridge&) = delete;

  ~SyncedBookmarksObserverBridge() override;
  // Returns true if it is undergoing the first sync cycle.
  bool IsPerformingInitialSync();

 private:
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<ChromeBrowserState> browser_state_;
};

}  // namespace sync_bookmarks

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_SYNCED_BOOKMARKS_BRIDGE_H_
