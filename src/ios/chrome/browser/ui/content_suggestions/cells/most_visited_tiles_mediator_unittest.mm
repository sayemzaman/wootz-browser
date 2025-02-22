// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_mediator.h"

#import "components/ntp_tiles/icon_cacher.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Testing Suite for MostVisitedTilesMediator
class MostVisitedTilesMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();

    favicon::LargeIconService* large_icon_service =
        IOSChromeLargeIconServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    LargeIconCache* cache = IOSChromeLargeIconCacheFactory::GetForBrowserState(
        chrome_browser_state_.get());
    std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_sites =
        std::make_unique<ntp_tiles::MostVisitedSites>(
            &pref_service_, /*top_sites*/ nullptr, /*popular_sites*/ nullptr,
            /*custom_links*/ nullptr, /*icon_cacher*/ nullptr,
            /*supervisor=*/nullptr, true);

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    mediator_ = [[MostVisitedTilesMediator alloc]
        initWithMostVisitedSite:std::move(most_visited_sites)
                    prefService:&pref_service_
               largeIconService:large_icon_service
                 largeIconCache:cache
         URLLoadingBrowserAgent:url_loader_];

    metrics_recorder_ = [[ContentSuggestionsMetricsRecorder alloc]
        initWithLocalState:local_state_.Get()];
    mediator_.contentSuggestionsMetricsRecorder = metrics_recorder_;
    mediator_.NTPMetricsDelegate =
        OCMProtocolMock(@protocol(NewTabPageMetricsDelegate));
  }
  ~MostVisitedTilesMediatorTest() override { [mediator_ disconnect]; }

 protected:
  web::WebTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  FakeUrlLoadingBrowserAgent* url_loader_;
  MostVisitedTilesMediator* mediator_;
  ContentSuggestionsMetricsRecorder* metrics_recorder_;
};

// Tests that the command is sent to the loader when opening a most visited.
TEST_F(MostVisitedTilesMediatorTest, TestOpenMostVisited) {
  GURL url = GURL("http://chromium.org");
  ContentSuggestionsMostVisitedItem* item =
      [[ContentSuggestionsMostVisitedItem alloc] init];
  item.URL = url;
  ContentSuggestionsMostVisitedTileView* view =
      [[ContentSuggestionsMostVisitedTileView alloc]
          initWithConfiguration:item];
  UIGestureRecognizer* recognizer = [[UIGestureRecognizer alloc] init];
  [view addGestureRecognizer:recognizer];
  OCMExpect([mediator_.NTPMetricsDelegate mostVisitedTileOpened]);

  // Action.
  [mediator_ mostVisitedTileTapped:recognizer];

  // Test.
  EXPECT_EQ(url, url_loader_->last_params.web_params.url);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      url_loader_->last_params.web_params.transition_type));
}
