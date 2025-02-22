// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_header_view_controller.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_constants.h"
#import "ios/chrome/browser/ui/ntp/feed_control_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_menu_commands.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Leading margin for title label. Its used to align with the Card leading
// margin.
const CGFloat kTitleHorizontalMargin = 19;
// Trailing/leading margins for header buttons. Its used to align with the card
// margins.
const CGFloat kButtonHorizontalMargin = 14;
// Font size for the custom search engine label.
const CGFloat kCustomSearchEngineLabelFontSize = 13;
// Font size for the hidden feed label.
const CGFloat kHiddenFeedLabelFontSize = 16;
// The width of the label for when the feed is hidden.
const CGFloat kHiddenFeedLabelWidth = 250;
// Insets for header menu button.
const CGFloat kHeaderManagementButtonInset = 2;
// The height of the header container. The content is unaffected.
const CGFloat kDiscoverFeedHeaderHeight = 40;
const CGFloat kCustomSearchEngineLabelHeight = 18;
// * Values below are exclusive to Web Channels.
// The height and width of the header menu button. Based on the default
// UISegmentedControl height.
const CGFloat kButtonSize = 28;
// The radius of the dot indicating that there is new content in the Following
// feed.
const CGFloat kFollowingDotRadius = 3;
// The distance between the Following segment dot and the Following label.
const CGFloat kFollowingDotMargin = 8;
// Duration of the fade animation for elements that toggle when switching feeds.
const CGFloat kSegmentAnimationDuration = 0.3;
// Padding on top of the header.
const CGFloat kTopVerticalPaddingFollowing = 15;
const CGFloat kTopVerticalPadding = 5;

// The size of feed symbol images.
NSInteger kFeedSymbolPointSize = 17;

}  // namespace

@interface FeedHeaderViewController ()

// View containing elements of the header. Handles header sizing.
@property(nonatomic, strong) UIView* container;

// Title label element for the feed.
@property(nonatomic, strong) UILabel* titleLabel;

// Button for opening top-level feed management menu.
// Redefined to not be readonly.
@property(nonatomic, strong) UIButton* managementButton;

// Button for sorting feed content. Only used for Following feed.
@property(nonatomic, strong) UIButton* sortButton;

// Segmented control for toggling between the two feeds.
@property(nonatomic, strong) UISegmentedControl* segmentedControl;

// The dot in the Following segment indicating new content in the Following
// feed.
@property(nonatomic, strong) UIView* followingDot;

// The blurred background of the feed header.
@property(nonatomic, strong) UIVisualEffectView* blurBackgroundView;

// The view informing the user that the feed is powered by Google if they don't
// have Google as their default search engine.
@property(nonatomic, strong) UILabel* customSearchEngineView;

// The label for when the feed visibility is disabled.
@property(nonatomic, strong) UILabel* hiddenFeedLabel;

// The constraints for the currently visible components of the header.
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* feedHeaderConstraints;

// Whether the Following segment dot should currently be visible.
@property(nonatomic, assign) BOOL followingDotVisible;

@end

@implementation FeedHeaderViewController

- (instancetype)initWithFollowingDotVisible:(BOOL)followingDotVisible {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _followingDotVisible = followingDotVisible;

    // The menu button is created early so that it can be assigned a tap action
    // before the view loads.
    _managementButton = [[UIButton alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // The background color will be clear for continuity with the overall NTP
  // gradient view.
  self.view.backgroundColor = [UIColor clearColor];
  self.view.maximumContentSizeCategory =
      UIContentSizeCategoryAccessibilityMedium;

  self.container = [[UIView alloc] init];

  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.container.translatesAutoresizingMaskIntoConstraints = NO;

  [self configureManagementButton:self.managementButton];
  [self configureHeaderViews];

  [self.view addSubview:self.container];
  [self applyHeaderConstraints];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    [self updateSegmentedControlFont:self.segmentedControl];
  } else {
    UIFont* font = [self fontForTitleLabel];
    self.titleLabel.font = font;
  }
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    [self.view setNeedsLayout];
  }
}

#pragma mark - Public

- (void)toggleBackgroundBlur:(BOOL)blurred animated:(BOOL)animated {
  if (UIAccessibilityIsReduceTransparencyEnabled() ||
      ![self.feedControlDelegate isFollowingFeedAvailable] ||
      !self.blurBackgroundView) {
    return;
  }

  // Applies blur to header background.
  if (!animated) {
    self.blurBackgroundView.hidden = !blurred;
    self.view.backgroundColor = [self backgroundColorForBlurredState:blurred];
    return;
  }
  [UIView transitionWithView:self.blurBackgroundView
      duration:0.3
      options:UIViewAnimationOptionTransitionCrossDissolve
      animations:^{
        self.blurBackgroundView.hidden = !blurred;
      }
      completion:^(BOOL finished) {
        // Only reduce opacity after the animation is complete to avoid showing
        // content suggestions tiles momentarily.
        self.view.backgroundColor =
            [self backgroundColorForBlurredState:blurred];
      }];
}

- (CGFloat)feedHeaderHeight {
  return [self.feedControlDelegate isFollowingFeedAvailable]
             ? FollowingFeedHeaderHeight()
             : kDiscoverFeedHeaderHeight;
}

- (CGFloat)customSearchEngineViewHeight {
  return [self.NTPDelegate isGoogleDefaultSearchEngine] ||
                 ![self.feedControlDelegate isFollowingFeedAvailable]
             ? 0
             : kCustomSearchEngineLabelHeight;
}

- (void)updateFollowingDotForUnseenContent:(BOOL)hasUnseenContent {
  DCHECK([self.feedControlDelegate isFollowingFeedAvailable]);

  // Don't show the dot if the user is already on the Following feed.
  if ([self.feedControlDelegate selectedFeed] == FeedTypeFollowing) {
    self.followingDotVisible = NO;
    return;
  }

  self.followingDotVisible = hasUnseenContent;

  [UIView animateWithDuration:kSegmentAnimationDuration
                   animations:^{
                     self.followingDot.alpha = hasUnseenContent ? 1 : 0;
                   }];
}

- (void)updateForDefaultSearchEngineChanged {
  if (!self.viewLoaded) {
    return;
  }
  if (![self.feedControlDelegate isFollowingFeedAvailable]) {
    [self.titleLabel setText:[self feedHeaderTitleText]];
    [self.titleLabel setNeedsDisplay];
    return;
  }

  if ([self.NTPDelegate isGoogleDefaultSearchEngine]) {
    [self removeCustomSearchEngineView];
  } else {
    [self addCustomSearchEngineView];
  }
  [self applyHeaderConstraints];
}

- (void)updateForFeedVisibilityChanged {
  // When feed visibility changes, the menu content is recreated.
  [self.feedMenuHandler configureManagementMenu:self.managementButton];

  if (![self.feedControlDelegate isFollowingFeedAvailable]) {
    [self.titleLabel setText:[self feedHeaderTitleText]];
    [self.titleLabel setNeedsDisplay];
    return;
  }

  if ([self.feedControlDelegate shouldFeedBeVisible]) {
    [self removeViewsForHiddenFeed];
    [self addViewsForVisibleFeed];
  } else {
    [self removeViewsForVisibleFeed];
    [self addViewsForHiddenFeed];
  }

  [self applyHeaderConstraints];
}

- (void)updateForFollowingFeedVisibilityChanged {
  [self removeViewsForHiddenFeed];
  [self removeViewsForVisibleFeed];
  [self.titleLabel removeFromSuperview];

  // The management button is different for the Following feed header, so it's
  // recreated.
  if (self.managementButton) {
    [self.managementButton removeFromSuperview];
    self.managementButton = nil;
  }
  self.managementButton = [[UIButton alloc] init];
  [self configureManagementButton:self.managementButton];
  [self.feedMenuHandler configureManagementMenu:self.managementButton];

  [self configureHeaderViews];
  [self applyHeaderConstraints];
}

- (void)updateForSelectedFeed {
  FeedType selectedFeed = [self.feedControlDelegate selectedFeed];
  self.segmentedControl.selectedSegmentIndex =
      static_cast<NSInteger>(selectedFeed);
  if (!IsFollowUIUpdateEnabled()) {
    self.sortButton.alpha = selectedFeed == FeedTypeDiscover ? 0 : 1;
  }
}

#pragma mark - Setters

// Sets `followingFeedSortType` and recreates the sort menu to assign the active
// sort type.
- (void)setFollowingFeedSortType:(FollowingFeedSortType)followingFeedSortType {
  CHECK(!IsFollowUIUpdateEnabled());
  _followingFeedSortType = followingFeedSortType;
  if (self.sortButton) {
    self.sortButton.menu = [self createSortMenu];
  }
}

#pragma mark - Private

- (void)configureHeaderViews {
  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    if ([self.feedControlDelegate shouldFeedBeVisible]) {
      [self addViewsForVisibleFeed];
    } else {
      [self addViewsForHiddenFeed];
    }

    if (![self.NTPDelegate isGoogleDefaultSearchEngine]) {
      [self addCustomSearchEngineView];
    }
  } else {
    self.titleLabel = [self createTitleLabel];
    [self.container addSubview:self.titleLabel];
  }
  [self.feedMenuHandler configureManagementMenu:self.managementButton];
}

// Creates sort menu with its content and active sort type.
- (UIMenu*)createSortMenu {
  NSMutableArray<UIAction*>* sortActions = [NSMutableArray array];

  // Create menu actions.
  UIAction* sortByPublisherAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_FEED_SORT_PUBLISHER)
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                [self.feedControlDelegate handleSortTypeForFollowingFeed:
                                              FollowingFeedSortTypeByPublisher];
              }];
  [sortActions addObject:sortByPublisherAction];
  UIAction* sortByLatestAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_FEED_SORT_LATEST)
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                [self.feedControlDelegate handleSortTypeForFollowingFeed:
                                              FollowingFeedSortTypeByLatest];
              }];
  [sortActions addObject:sortByLatestAction];

  // Set active sorting.
  switch (self.followingFeedSortType) {
    case FollowingFeedSortTypeByLatest:
      sortByLatestAction.state = UIMenuElementStateOn;
      break;
    case FollowingFeedSortTypeByPublisher:
    default:
      sortByPublisherAction.state = UIMenuElementStateOn;
  }

  return [UIMenu menuWithTitle:@"" children:sortActions];
}

// Configures the feed header's menu button.
- (void)configureManagementButton:(UIButton*)managementButton {
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];

  managementButton.translatesAutoresizingMaskIntoConstraints = NO;
  managementButton.showsMenuAsPrimaryAction = YES;
  [managementButton addTarget:self.feedMenuHandler
                       action:@selector(configureManagementMenu:)
             forControlEvents:UIControlEventTouchDown];

  managementButton.accessibilityIdentifier =
      kNTPFeedHeaderManagementButtonIdentifier;
  managementButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_MENU_ACCESSIBILITY_LABEL);

  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    buttonConfiguration.image =
        DefaultSymbolTemplateWithPointSize(kMenuSymbol, kFeedSymbolPointSize);
    if (!IsFeedContainmentEnabled()) {
      buttonConfiguration.background.backgroundColor =
          [[UIColor colorNamed:kGrey200Color] colorWithAlphaComponent:0.8];
      managementButton.layer.cornerRadius = kButtonSize / 2;
    }
    managementButton.clipsToBounds = YES;
  } else {
    UIImage* menuIcon = DefaultSymbolTemplateWithPointSize(
        kSettingsFilledSymbol, kFeedSymbolPointSize);
    buttonConfiguration.image = menuIcon;
    buttonConfiguration.baseForegroundColor =
        [UIColor colorNamed:kGrey600Color];
    buttonConfiguration.imagePadding = kHeaderManagementButtonInset;
  }

  [self.container addSubview:managementButton];
  managementButton.configuration = buttonConfiguration;
}

// Configures and returns the feed header's sorting button.
- (UIButton*)createSortButton {
  DCHECK([self.feedControlDelegate isFollowingFeedAvailable]);

  UIButton* sortButton = [[UIButton alloc] init];

  sortButton.translatesAutoresizingMaskIntoConstraints = NO;
  sortButton.accessibilityIdentifier = kNTPFeedHeaderSortButtonIdentifier;
  sortButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_FEED_SORT_ACCESSIBILITY_LABEL);
  [sortButton setImage:DefaultSymbolTemplateWithPointSize(kSortSymbol,
                                                          kFeedSymbolPointSize)
              forState:UIControlStateNormal];
  sortButton.showsMenuAsPrimaryAction = YES;

  // The sort button is only visible if the Following feed is selected, and if
  // the Follow UI update is not enabled.
  if (IsFollowUIUpdateEnabled()) {
    sortButton.alpha = 0;
  } else {
    sortButton.alpha =
        [self.feedControlDelegate selectedFeed] == FeedTypeDiscover ? 0 : 1;
  }

  sortButton.configuration = [UIButtonConfiguration plainButtonConfiguration];

  return sortButton;
}

// Configures and returns the feed header's title label.
- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [self fontForTitleLabel];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.accessibilityIdentifier =
      ntp_home::DiscoverHeaderTitleAccessibilityID();
  titleLabel.text = [self feedHeaderTitleText];
  return titleLabel;
}

// Configures and returns the segmented control for toggling between feeds.
- (UISegmentedControl*)createSegmentedControl {
  // Create segmented control with labels.
  NSArray* headerLabels = [NSArray
      arrayWithObjects:l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE),
                       l10n_util::GetNSString(IDS_IOS_FOLLOWING_FEED_TITLE),
                       nil];
  UISegmentedControl* segmentedControl =
      [[UISegmentedControl alloc] initWithItems:headerLabels];
  segmentedControl.translatesAutoresizingMaskIntoConstraints = NO;
  [segmentedControl setApportionsSegmentWidthsByContent:NO];

  [self updateSegmentedControlFont:segmentedControl];

  // Set selected feed and tap action.
  segmentedControl.selectedSegmentIndex =
      static_cast<NSInteger>([self.feedControlDelegate selectedFeed]);
  [segmentedControl addTarget:self
                       action:@selector(onSegmentSelected:)
             forControlEvents:UIControlEventValueChanged];

  segmentedControl.accessibilityIdentifier =
      kNTPFeedHeaderSegmentedControlIdentifier;

  return segmentedControl;
}

- (UIFont*)fontForTitleLabel {
  return CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold,
                           self.view);
}

- (UIFont*)fontForSegmentedControl {
  return CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightMedium,
                           self.view);
}

// Configures and returns the dot indicating that there is new content in the
// Following feed.
- (UIView*)createFollowingDot {
  UIView* followingDot = [[UIView alloc] init];
  followingDot.layer.cornerRadius = kFollowingDotRadius;
  followingDot.backgroundColor = [UIColor colorNamed:kBlue500Color];
  followingDot.translatesAutoresizingMaskIntoConstraints = NO;
  return followingDot;
}

// Configures and returns the blurred background of the feed header.
- (UIVisualEffectView*)createBlurBackground {
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
  UIVisualEffectView* blurBackgroundView =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  blurBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  return blurBackgroundView;
}

// Configures and returns the label for when the feed visibility is
// disabled.
- (UILabel*)createHiddenFeedLabel {
  UILabel* hiddenFeedLabel = [[UILabel alloc] init];
  [hiddenFeedLabel setText:l10n_util::GetNSString(
                               IDS_IOS_DISCOVER_FEED_HEADER_TURNED_OFF_LABEL)];
  hiddenFeedLabel.translatesAutoresizingMaskIntoConstraints = NO;
  hiddenFeedLabel.font = [UIFont systemFontOfSize:kHiddenFeedLabelFontSize];
  hiddenFeedLabel.textColor = [UIColor colorNamed:kGrey600Color];
  hiddenFeedLabel.numberOfLines = 0;
  hiddenFeedLabel.textAlignment = NSTextAlignmentCenter;
  return hiddenFeedLabel;
}

// Updates the font and color of the segmented control header to adapt to the
// current dynamic sizing.
- (void)updateSegmentedControlFont:(UISegmentedControl*)segmentedControl {
  NSDictionary* normalAttributes = [NSDictionary
      dictionaryWithObjectsAndKeys:[self fontForSegmentedControl],
                                   NSFontAttributeName,
                                   [UIColor colorNamed:kTextSecondaryColor],
                                   NSForegroundColorAttributeName, nil];
  [segmentedControl setTitleTextAttributes:normalAttributes
                                  forState:UIControlStateNormal];
  NSDictionary* selectedAttributes = [NSDictionary
      dictionaryWithObjectsAndKeys:[self fontForSegmentedControl],
                                   NSFontAttributeName,
                                   [UIColor colorNamed:kTextPrimaryColor],
                                   NSForegroundColorAttributeName, nil];
  [segmentedControl setTitleTextAttributes:selectedAttributes
                                  forState:UIControlStateSelected];
}

- (void)addCustomSearchEngineView {
  if (self.customSearchEngineView) {
    [self removeCustomSearchEngineView];
  }
  self.customSearchEngineView = [[UILabel alloc] init];
  self.customSearchEngineView.text =
      l10n_util::GetNSString(IDS_IOS_FEED_CUSTOM_SEARCH_ENGINE_LABEL);
  self.customSearchEngineView.font =
      [UIFont systemFontOfSize:kCustomSearchEngineLabelFontSize];
  self.customSearchEngineView.textColor = [UIColor colorNamed:kGrey500Color];
  self.customSearchEngineView.translatesAutoresizingMaskIntoConstraints = NO;
  self.customSearchEngineView.textAlignment = NSTextAlignmentCenter;
  [self.view addSubview:self.customSearchEngineView];
}

- (void)removeCustomSearchEngineView {
  [self.customSearchEngineView removeFromSuperview];
  self.customSearchEngineView = nil;
}

// Applies constraints for the feed header elements' positioning.
- (void)applyHeaderConstraints {
  // Remove previous constraints if they were already set.
  if (self.feedHeaderConstraints) {
    [NSLayoutConstraint deactivateConstraints:self.feedHeaderConstraints];
    self.feedHeaderConstraints = nil;
  }

  self.feedHeaderConstraints = [[NSMutableArray alloc] init];

  CGFloat totalHeaderHeight =
      [self feedHeaderHeight] + [self customSearchEngineViewHeight];
  if (IsFeedContainmentEnabled()) {
    totalHeaderHeight += [self.feedControlDelegate isFollowingFeedAvailable]
                             ? kTopVerticalPaddingFollowing
                             : kTopVerticalPadding;
  }
  CGFloat buttonMargin =
      IsFeedContainmentEnabled() ? kButtonHorizontalMargin : 0;
  [self.feedHeaderConstraints addObjectsFromArray:@[
    // Anchor container and menu button.
    [self.view.heightAnchor constraintEqualToConstant:totalHeaderHeight],
    [self.container.heightAnchor
        constraintEqualToConstant:[self feedHeaderHeight]],
    [self.container.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.container.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.container.widthAnchor constraintEqualToAnchor:self.view.widthAnchor],
    [self.managementButton.trailingAnchor
        constraintEqualToAnchor:self.container.trailingAnchor
                       constant:-buttonMargin],
    [self.managementButton.centerYAnchor
        constraintEqualToAnchor:self.container.centerYAnchor],
    // Set menu button size.
    [self.managementButton.heightAnchor constraintEqualToConstant:kButtonSize],
    [self.managementButton.widthAnchor constraintEqualToConstant:kButtonSize],
  ]];

  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    // Anchor views based on the feed being visible or hidden.
    if ([self.feedControlDelegate shouldFeedBeVisible]) {
      [self anchorSegmentedControlAndDot];

      // Anchor sort button.
      [self.feedHeaderConstraints addObjectsFromArray:@[
        [self.sortButton.heightAnchor constraintEqualToConstant:kButtonSize],
        [self.sortButton.widthAnchor constraintEqualToConstant:kButtonSize],
        [self.sortButton.leadingAnchor
            constraintEqualToAnchor:self.container.leadingAnchor
                           constant:kButtonHorizontalMargin],
        [self.sortButton.centerYAnchor
            constraintEqualToAnchor:self.container.centerYAnchor],
      ]];

      // Anchor blur background view if reduce transparency is disabled.
      if (self.blurBackgroundView) {
        [self.feedHeaderConstraints addObjectsFromArray:@[
          [self.blurBackgroundView.trailingAnchor
              constraintEqualToAnchor:self.view.trailingAnchor],
          [self.blurBackgroundView.leadingAnchor
              constraintEqualToAnchor:self.view.leadingAnchor],
          [self.blurBackgroundView.topAnchor
              constraintEqualToAnchor:self.container.topAnchor],
          [self.blurBackgroundView.bottomAnchor
              constraintEqualToAnchor:self.container.bottomAnchor],
        ]];
      }
    } else {
      [self.feedHeaderConstraints addObjectsFromArray:@[
        [self.hiddenFeedLabel.centerXAnchor
            constraintEqualToAnchor:self.container.centerXAnchor],
        [self.hiddenFeedLabel.centerYAnchor
            constraintEqualToAnchor:self.container.centerYAnchor],
        [self.hiddenFeedLabel.widthAnchor
            constraintEqualToConstant:kHiddenFeedLabelWidth],
      ]];
    }

    // If Google is not the default search engine, anchor the custom search
    // engine view.
    if (![self.NTPDelegate isGoogleDefaultSearchEngine] &&
        [self.feedControlDelegate shouldFeedBeVisible]) {
      [self.feedHeaderConstraints addObjectsFromArray:@[
        // Anchors custom search engine view.
        [self.customSearchEngineView.widthAnchor
            constraintEqualToAnchor:self.view.widthAnchor],
        [self.customSearchEngineView.heightAnchor
            constraintEqualToConstant:kCustomSearchEngineLabelHeight],
        [self.customSearchEngineView.bottomAnchor
            constraintEqualToAnchor:self.container.topAnchor],
      ]];
    }

  } else {
    CGFloat titleMargin =
        IsFeedContainmentEnabled() ? kTitleHorizontalMargin : 0;
    [self.feedHeaderConstraints addObjectsFromArray:@[
      // Anchors title label.
      [self.titleLabel.leadingAnchor
          constraintEqualToAnchor:self.container.leadingAnchor
                         constant:titleMargin],
      [self.titleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.managementButton
                                                .leadingAnchor],
      [self.titleLabel.centerYAnchor
          constraintEqualToAnchor:self.container.centerYAnchor],
    ]];
  }
  [NSLayoutConstraint activateConstraints:self.feedHeaderConstraints];
}

// Anchors the segmented control and the unseen content dot.
- (void)anchorSegmentedControlAndDot {
  // Anchor segmented control.
  [self.feedHeaderConstraints addObjectsFromArray:@[
    [self.segmentedControl.centerXAnchor
        constraintEqualToAnchor:self.container.centerXAnchor],
    [self.segmentedControl.centerYAnchor
        constraintEqualToAnchor:self.container.centerYAnchor],
    [self.segmentedControl.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.managementButton.leadingAnchor
                                 constant:-kButtonHorizontalMargin],
    [self.segmentedControl.leadingAnchor
        constraintEqualToAnchor:self.sortButton.trailingAnchor
                       constant:kButtonHorizontalMargin],
  ]];

  // Set Following segment dot size.
  [self.feedHeaderConstraints addObjectsFromArray:@[
    [self.followingDot.heightAnchor
        constraintEqualToConstant:kFollowingDotRadius * 2],
    [self.followingDot.widthAnchor
        constraintEqualToConstant:kFollowingDotRadius * 2],
  ]];

  // Find the "Following" label within the segmented control, since it is not
  // exposed by UISegmentedControl. First loop iterates through UISegments, and
  // next loop iterates to find their nested UISegmentLabels.
  UILabel* followingLabel;
  for (UIView* view in self.segmentedControl.subviews) {
    for (UIView* subview in view.subviews) {
      if ([NSStringFromClass([subview class])
              isEqualToString:@"UISegmentLabel"]) {
        UILabel* currentLabel = static_cast<UILabel*>(subview);
        if ([currentLabel.text
                isEqualToString:l10n_util::GetNSString(
                                    IDS_IOS_FOLLOWING_FEED_TITLE)]) {
          followingLabel = currentLabel;
          break;
        }
      }
    }
  }

  // If the label was found, anchor the dot to it. Otherwise, anchor the dot
  // to the top corner of the segmented control.
  if (followingLabel) {
    [self.feedHeaderConstraints addObjectsFromArray:@[
      // Anchor Following segment dot to label text.
      [self.followingDot.leftAnchor
          constraintEqualToAnchor:followingLabel.rightAnchor
                         constant:kFollowingDotMargin],
      [self.followingDot.bottomAnchor
          constraintEqualToAnchor:followingLabel.topAnchor
                         constant:kFollowingDotMargin],
    ]];
  } else {
    [self.feedHeaderConstraints addObjectsFromArray:@[
      // Anchor Following segment dot to top corner.
      [self.followingDot.rightAnchor
          constraintEqualToAnchor:self.segmentedControl.rightAnchor
                         constant:-kFollowingDotMargin],
      [self.followingDot.topAnchor
          constraintEqualToAnchor:self.segmentedControl.topAnchor
                         constant:kFollowingDotMargin],
    ]];
  }
}

// Adds views that only appear when the feed visibility is enabled.
- (void)addViewsForVisibleFeed {
  self.segmentedControl = [self createSegmentedControl];
  [self.container addSubview:self.segmentedControl];

  self.followingDot = [self createFollowingDot];
  self.followingDot.alpha = self.followingDotVisible ? 1 : 0;
  [self.segmentedControl addSubview:self.followingDot];

  self.sortButton = [self createSortButton];
  // If the Follow UI update is enabled, we still create the sort button to help
  // anchor the segmented control. However, the sort button remains invisible
  // and the menu is not created.
  if (!IsFollowUIUpdateEnabled()) {
    self.sortButton.menu = [self createSortMenu];
  }

  [self.container addSubview:self.sortButton];

  if (!UIAccessibilityIsReduceTransparencyEnabled()) {
    self.blurBackgroundView = [self createBlurBackground];
    [self.view addSubview:self.blurBackgroundView];
    [self.view sendSubviewToBack:self.blurBackgroundView];

    // The blurred background has a tint that is visible when the header is
    // over the standard NTP background. For this reason, we only add the blur
    // background when scrolled into the feed.
    self.blurBackgroundView.hidden = YES;
  }

  if (![self.NTPDelegate isGoogleDefaultSearchEngine]) {
    [self addCustomSearchEngineView];
  }
}

// Adds views that only appear when the feed visibility is disabled.
- (void)addViewsForHiddenFeed {
  self.hiddenFeedLabel = [self createHiddenFeedLabel];
  [self.container addSubview:self.hiddenFeedLabel];
}

// Removes views that only appear when the feed visibility is enabled.
- (void)removeViewsForVisibleFeed {
  if (self.followingDot) {
    [self.followingDot removeFromSuperview];
    self.followingDot = nil;
  }

  if (self.segmentedControl) {
    [self.segmentedControl removeFromSuperview];
    self.segmentedControl = nil;
  }

  if (self.sortButton) {
    [self.sortButton removeFromSuperview];
    self.sortButton = nil;
  }

  if (self.customSearchEngineView) {
    [self removeCustomSearchEngineView];
  }
}

// Removes views that only appear when the feed visibility is disabled.
- (void)removeViewsForHiddenFeed {
  if (self.hiddenFeedLabel) {
    [self.hiddenFeedLabel removeFromSuperview];
    self.hiddenFeedLabel = nil;
  }
}

// Handles a new feed being selected from the header.
- (void)onSegmentSelected:(UISegmentedControl*)segmentedControl {
  switch (segmentedControl.selectedSegmentIndex) {
    case static_cast<NSInteger>(FeedTypeDiscover): {
      [self.feedMetricsRecorder
                recordFeedSelected:FeedTypeDiscover
          fromPreviousFeedPosition:[self.feedControlDelegate
                                           lastVisibleFeedCardIndex]];
      [self.feedControlDelegate handleFeedSelected:FeedTypeDiscover];
      [UIView animateWithDuration:kSegmentAnimationDuration
                       animations:^{
                         self.sortButton.alpha = 0;
                       }];
      break;
    }
    case static_cast<NSInteger>(FeedTypeFollowing): {
      [self.feedMetricsRecorder
                recordFeedSelected:FeedTypeFollowing
          fromPreviousFeedPosition:[self.feedControlDelegate
                                           lastVisibleFeedCardIndex]];
      [self.feedControlDelegate handleFeedSelected:FeedTypeFollowing];
      // Only show sorting button for Following feed.
      [UIView animateWithDuration:kSegmentAnimationDuration
                       animations:^{
                         self.sortButton.alpha =
                             IsFollowUIUpdateEnabled() ? 0 : 1;
                       }];
      break;
    }
  }
}

// The title text for the Discover feed header based on user prefs.
- (NSString*)feedHeaderTitleText {
  DCHECK(![self.feedControlDelegate isFollowingFeedAvailable]);

  // Set the title based on the default search engine.
  NSString* feedHeaderTitleText =
      [self.NTPDelegate isGoogleDefaultSearchEngine]
          ? l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE)
          : l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE_NON_DSE);

  // Append the title text if the feed is turned off.
  if (![self.feedControlDelegate shouldFeedBeVisible]) {
    feedHeaderTitleText =
        [NSString stringWithFormat:@"%@ – %@", feedHeaderTitleText,
                                   l10n_util::GetNSString(
                                       IDS_IOS_DISCOVER_FEED_TITLE_OFF_LABEL)];
  }

  return feedHeaderTitleText;
}

// Returns the background color for this view.
// Applies an opacity to the background. If ReduceTransparency is enabled,
// then this replaces the blur effect.
// With the Magic Stack enabled, the background color will
// be clear for continuity with the overall NTP gradient view.
- (UIColor*)backgroundColorForBlurredState:(BOOL)blurred {
  if (blurred) {
    return [[UIColor colorNamed:kBackgroundColor] colorWithAlphaComponent:0.1];
  } else {
    return [UIColor clearColor];
  }
}

@end
