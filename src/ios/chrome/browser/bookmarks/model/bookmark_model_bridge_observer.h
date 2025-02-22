// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_MODEL_BRIDGE_OBSERVER_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_MODEL_BRIDGE_OBSERVER_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"

class LegacyBookmarkModel;

// The ObjC translations of the C++ observer callbacks are defined here.
@protocol BookmarkModelBridgeObserver <NSObject>
// The bookmark model has loaded.
- (void)bookmarkModelLoaded:(LegacyBookmarkModel*)model;
// The node has changed, but not its children.
- (void)bookmarkModel:(LegacyBookmarkModel*)model
        didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode;
// The node has not changed, but the ordering and existence of its children have
// changed.
- (void)bookmarkModel:(LegacyBookmarkModel*)model
    didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode;
// The node has moved to a new parent folder.
- (void)bookmarkModel:(LegacyBookmarkModel*)model
          didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
           fromParent:(const bookmarks::BookmarkNode*)oldParent
             toParent:(const bookmarks::BookmarkNode*)newParent;
// `node` was deleted from `folder`.
- (void)bookmarkModel:(LegacyBookmarkModel*)model
        didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder;
// All non-permanent nodes have been removed in model.
- (void)bookmarkModelRemovedAllNodes:(LegacyBookmarkModel*)model;

@optional
// Called before removing a bookmark node.
- (void)bookmarkModel:(LegacyBookmarkModel*)model
       willDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder;
// Called after adding a bookmark node.
- (void)bookmarkModel:(LegacyBookmarkModel*)model
           didAddNode:(const bookmarks::BookmarkNode*)node
             toFolder:(const bookmarks::BookmarkNode*)folder;

// Called before removing all non-permanent nodes.
- (void)bookmarkModelWillRemoveAllNodes:(const LegacyBookmarkModel*)model;
// The node favicon changed.
- (void)bookmarkModel:(LegacyBookmarkModel*)model
    didChangeFaviconForNode:(const bookmarks::BookmarkNode*)bookmarkNode;
// Called before changing a bookmark node.
- (void)bookmarkModel:(LegacyBookmarkModel*)model
    willChangeBookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode;
// Called when the model is being deleted.
- (void)bookmarkModelBeingDeleted:(LegacyBookmarkModel*)model;
@end

// A bridge that translates BookmarkModelObserver C++ callbacks into ObjC
// callbacks.
class BookmarkModelBridge : public bookmarks::BookmarkModelObserver {
 public:
  explicit BookmarkModelBridge(id<BookmarkModelBridgeObserver> observer,
                               LegacyBookmarkModel* model);
  ~BookmarkModelBridge() override;

 private:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void OnWillRemoveBookmarks(const bookmarks::BookmarkNode* parent,
                             size_t old_index,
                             const bookmarks::BookmarkNode* node,
                             const base::Location& location) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;

  void OnWillChangeBookmarkNode(const bookmarks::BookmarkNode* node) override;

  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void OnWillRemoveAllUserBookmarks(const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;

  __weak id<BookmarkModelBridgeObserver> observer_;

  base::ScopedObservation<LegacyBookmarkModel, bookmarks::BookmarkModelObserver>
      model_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_MODEL_BRIDGE_OBSERVER_H_
