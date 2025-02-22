// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/ui/price_insights_cell.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_constants.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_item.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

const char kUrl[] = "https://www.merchant.com/price_drop_product";

NSString* kTitle = @"Product title";
NSString* kVariant = @"Product variant";
NSString* kLowPrice = @"$699";
NSString* kHighPrice = @"$799";
NSString* kCurrency = @"USD";

// Retrieves a view of a specified class with a given accessibility identifier
// within a given view hierarchy.
UIView* GetViewOfClassWithIdentifier(Class ui_class,
                                     NSString* accessibility_id,
                                     UIView* view) {
  if ([view isKindOfClass:ui_class] &&
      view.accessibilityIdentifier == accessibility_id) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    UIView* foundView =
        GetViewOfClassWithIdentifier(ui_class, accessibility_id, subview);
    if (foundView) {
      return foundView;
    }
  }
  return nil;
}

// Retrieves a UILabel with a given accessibility identifier within a given view
// hierarchy.
UILabel* GetLabelFromIdentifier(NSString* identifier, UIView* view) {
  return base::apple::ObjCCastStrict<UILabel>(
      GetViewOfClassWithIdentifier([UILabel class], identifier, view));
}

// Retrieves a UIStackView with a given accessibility identifier within a given
// view hierarchy.
UIStackView* GetStackViewFromIdentifier(NSString* identifier, UIView* view) {
  return base::apple::ObjCCastStrict<UIStackView>(
      GetViewOfClassWithIdentifier([UIStackView class], identifier, view));
}

// Creates a PriceInsightsItem based on the provided parameters.
PriceInsightsItem* GetPriceInsights(bool has_variant,
                                    bool has_tracking,
                                    bool has_range,
                                    bool has_single_range,
                                    bool has_history,
                                    bool has_buying_options) {
  PriceInsightsItem* item = [[PriceInsightsItem alloc] init];
  item.title = kTitle;
  item.variants = has_variant ? kVariant : nil;
  if (has_range) {
    item.lowPrice = kLowPrice;
    item.highPrice = kHighPrice;
  } else if (has_single_range) {
    item.lowPrice = kLowPrice;
    item.highPrice = item.lowPrice;
  }
  item.canPriceTrack = has_tracking;
  item.buyingOptionsURL = has_buying_options ? GURL(kUrl) : GURL();
  item.currency = kCurrency;

  if (has_history) {
    NSDateFormatter* dateFormat = [[NSDateFormatter alloc] init];
    [dateFormat setDateFormat:@"yyyy-MM-dd"];
    NSDate* date = [dateFormat dateFromString:@"2023-08-19"];
    item.priceHistory = @{
      date : [NSNumber numberWithInt:20],
    };
  }
  return item;
}

}  // namespace

class PriceInsightsCellTest : public PlatformTest {
 public:
  PriceInsightsCellTest() {}
  ~PriceInsightsCellTest() override {}

  void SetUp() override { cell_ = [[PriceInsightsCell alloc] init]; }

  // Checks the number of components within the cell's content view.
  void CheckNumberOfComponents(unsigned long count) {
    EXPECT_EQ(1ul, cell_.contentView.subviews.count);
    EXPECT_EQ(count, cell_.contentView.subviews[0].subviews.count);
  }

  // Checks the position of a component within the cell's content view.
  void CheckPositionOfComponent(NSString* accessibility_identifier,
                                int position) {
    UIStackView* stack_view = base::apple::ObjCCastStrict<UIStackView>(
        cell_.contentView.subviews[0].subviews[position]);
    EXPECT_EQ(stack_view.accessibilityIdentifier, accessibility_identifier);
  }

  PriceInsightsCell* cell_;
};

// Test that when no data is available, there are no components.
TEST_F(PriceInsightsCellTest, TestNoDataAvailable) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/false,
      /*has_tracking=*/false,
      /*has_range=*/false,
      /*has_single_range=*/false,
      /*has_history=*/false,
      /*has_buying_options=*/false);
  [cell_ configureWithItem:item];

  CheckNumberOfComponents(0ul);
}

// Test the order of components in the content view when Price Tracking is not
// available.
TEST_F(PriceInsightsCellTest, TestViewOrderWhenPriceTrackingNotAvailable) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/true,
      /*has_tracking=*/false,
      /*has_range=*/true,
      /*has_single_range=*/false,
      /*has_history=*/true,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  CheckNumberOfComponents(3ul);
  CheckPositionOfComponent(kPriceTrackingStackViewIdentifier, 0);
  CheckPositionOfComponent(kPriceHistoryStackViewIdentifier, 1);
  CheckPositionOfComponent(kBuyingOptionsStackViewIdentifier, 2);
}

// Test the order of components in the content view when Price History is the
// only missing data.
TEST_F(PriceInsightsCellTest, TestViewOrderWhenPriceHistoryNotAvailable) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/true,
      /*has_tracking=*/true,
      /*has_range=*/true,
      /*has_single_range=*/false,
      /*has_history=*/false,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  CheckNumberOfComponents(1ul);
  CheckPositionOfComponent(kPriceTrackingStackViewIdentifier, 0);
}

// Test the order of components in the content view when Price Range is the only
// missing data.
TEST_F(PriceInsightsCellTest, TestViewOrderWhenPriceRangeNotAvailable) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/true,
      /*has_tracking=*/true,
      /*has_range=*/false,
      /*has_single_range=*/false,
      /*has_history=*/true,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  CheckNumberOfComponents(2ul);
  CheckPositionOfComponent(kPriceTrackingStackViewIdentifier, 0);
  CheckPositionOfComponent(kPriceHistoryStackViewIdentifier, 1);
}

// Test the order of components in the content view when all the components are
// available.
TEST_F(PriceInsightsCellTest, TestViewOrderWhenAllComponentsAvailable) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/true,
      /*has_tracking=*/true,
      /*has_range=*/true,
      /*has_single_range=*/false,
      /*has_history=*/true,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  CheckNumberOfComponents(3ul);
  CheckPositionOfComponent(kPriceTrackingStackViewIdentifier, 0);
  CheckPositionOfComponent(kPriceHistoryStackViewIdentifier, 1);
  CheckPositionOfComponent(kBuyingOptionsStackViewIdentifier, 2);
}

// Tests the presence of UI elements in the Price Tracking components when only
// Price Tracking is available.
TEST_F(PriceInsightsCellTest, TestPriceTrackingUIOnly) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/false,
      /*has_tracking=*/true,
      /*has_range=*/false,
      /*has_single_range=*/false,
      /*has_history=*/false,
      /*has_buying_options=*/false);
  [cell_ configureWithItem:item];

  UILabel* title =
      GetLabelFromIdentifier(kPriceTrackingTitleIdentifier, cell_.contentView);
  EXPECT_TRUE([title.text isEqualToString:kTitle]);
  UILabel* subtitle = GetLabelFromIdentifier(kPriceTrackingSubtitleIdentifier,
                                             cell_.contentView);
  EXPECT_TRUE([subtitle.text
      isEqualToString:l10n_util::GetNSString(IDS_PRICE_TRACKING_DESCRIPTION)]);

  UIStackView* price_tracking_stack_view = GetStackViewFromIdentifier(
      kPriceTrackingStackViewIdentifier, cell_.contentView);
  UIButton* button = base::apple::ObjCCastStrict<UIButton>(
      [price_tracking_stack_view.subviews objectAtIndex:1]);
  EXPECT_EQ(button.accessibilityIdentifier, kPriceTrackingButtonIdentifier);
}

// Tests the presence of UI elements in the Price Tracking components when all
// data are available, but without any variants.
TEST_F(PriceInsightsCellTest, TestPriceTrackingUIWithHistoryRangeAndNoVariant) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/false,
      /*has_tracking=*/false,
      /*has_range=*/true,
      /*has_single_range=*/false,
      /*has_history=*/true,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  UILabel* title =
      GetLabelFromIdentifier(kPriceTrackingTitleIdentifier, cell_.contentView);
  EXPECT_TRUE([title.text isEqualToString:kTitle]);
  UILabel* subtitle = GetLabelFromIdentifier(kPriceTrackingSubtitleIdentifier,
                                             cell_.contentView);
  EXPECT_TRUE([subtitle.text
      isEqualToString:l10n_util::GetNSStringF(
                          IDS_PRICE_RANGE_SINGLE_OPTION,
                          base::SysNSStringToUTF16(item.lowPrice),
                          base::SysNSStringToUTF16(item.highPrice))]);
}

// Tests the presence of UI elements in the Price Tracking components when all
// data are available.
TEST_F(PriceInsightsCellTest, TestPriceTrackingUIHistoryRangeAndVariant) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/true,
      /*has_tracking=*/false,
      /*has_range=*/true,
      /*has_single_range=*/false,
      /*has_history=*/true,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  UILabel* title =
      GetLabelFromIdentifier(kPriceTrackingTitleIdentifier, cell_.contentView);
  EXPECT_TRUE([title.text isEqualToString:kTitle]);
  UILabel* subtitle = GetLabelFromIdentifier(kPriceTrackingSubtitleIdentifier,
                                             cell_.contentView);
  EXPECT_TRUE([subtitle.text
      isEqualToString:l10n_util::GetNSStringF(
                          IDS_PRICE_RANGE_ALL_OPTIONS,
                          base::SysNSStringToUTF16(item.lowPrice),
                          base::SysNSStringToUTF16(item.highPrice))]);
}

// Tests the presence of UI elements in the Price Tracking components when all
// data are available, but with single range.
TEST_F(PriceInsightsCellTest, TestPriceTrackingUIHistorySingleRangeAndVariant) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/true,
      /*has_tracking=*/false,
      /*has_range=*/false,
      /*has_single_range=*/true,
      /*has_history=*/true,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  UILabel* title =
      GetLabelFromIdentifier(kPriceTrackingTitleIdentifier, cell_.contentView);
  EXPECT_TRUE([title.text isEqualToString:kTitle]);
  UILabel* subtitle = GetLabelFromIdentifier(kPriceTrackingSubtitleIdentifier,
                                             cell_.contentView);
  EXPECT_TRUE([subtitle.text
      isEqualToString:l10n_util::GetNSStringF(
                          IDS_PRICE_RANGE_ALL_OPTIONS_ONE_TYPICAL_PRICE,
                          base::SysNSStringToUTF16(item.lowPrice))]);
}

// Tests the presence of UI elements in the Price Tracking components when all
// data are available, with single range, and without any variants.
TEST_F(PriceInsightsCellTest,
       TestPriceTrackingUIHistorySingleRangeAndNoVariant) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/false,
      /*has_tracking=*/false,
      /*has_range=*/false,
      /*has_single_range=*/true,
      /*has_history=*/true,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  UILabel* title =
      GetLabelFromIdentifier(kPriceTrackingTitleIdentifier, cell_.contentView);
  EXPECT_TRUE([title.text isEqualToString:kTitle]);
  UILabel* subtitle = GetLabelFromIdentifier(kPriceTrackingSubtitleIdentifier,
                                             cell_.contentView);
  EXPECT_TRUE([subtitle.text
      isEqualToString:l10n_util::GetNSStringF(
                          IDS_PRICE_RANGE_SINGLE_OPTION_ONE_TYPICAL_PRICE,
                          base::SysNSStringToUTF16(item.lowPrice))]);
}

// Tests the presence of UI elements in the Price Tracking components when Price
// History and Price Tracking are available, and without any variants.
TEST_F(PriceInsightsCellTest, TestPriceTrackingUIHistoryNoRangeAndNoVariant) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/false,
      /*has_tracking=*/true,
      /*has_range=*/false,
      /*has_single_range=*/false,
      /*has_history=*/true,
      /*has_buying_options=*/false);
  [cell_ configureWithItem:item];

  UILabel* title =
      GetLabelFromIdentifier(kPriceTrackingTitleIdentifier, cell_.contentView);
  EXPECT_TRUE([title.text isEqualToString:kTitle]);
  UILabel* subtitle = GetLabelFromIdentifier(kPriceTrackingSubtitleIdentifier,
                                             cell_.contentView);
  EXPECT_TRUE([subtitle.text
      isEqualToString:l10n_util::GetNSString(IDS_PRICE_TRACKING_DESCRIPTION)]);

  UIStackView* price_tracking_stack_view = GetStackViewFromIdentifier(
      kPriceTrackingStackViewIdentifier, cell_.contentView);
  UIButton* button = base::apple::ObjCCastStrict<UIButton>(
      [price_tracking_stack_view.subviews objectAtIndex:1]);
  EXPECT_EQ(button.accessibilityIdentifier, kPriceTrackingButtonIdentifier);
}

// Tests the presence of UI elements in the Price Tracking components when Price
// History and Price Tracking are available, and with variants.
TEST_F(PriceInsightsCellTest, TestPriceTrackingUIHistoryNoRangeAndVariant) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/true,
      /*has_tracking=*/true,
      /*has_range=*/false,
      /*has_single_range=*/false,
      /*has_history=*/true,
      /*has_buying_options=*/false);
  [cell_ configureWithItem:item];

  UILabel* title =
      GetLabelFromIdentifier(kPriceTrackingTitleIdentifier, cell_.contentView);
  EXPECT_TRUE([title.text isEqualToString:kTitle]);
  UILabel* subtitle = GetLabelFromIdentifier(kPriceTrackingSubtitleIdentifier,
                                             cell_.contentView);
  EXPECT_TRUE([subtitle.text
      isEqualToString:l10n_util::GetNSString(IDS_PRICE_TRACKING_DESCRIPTION)]);

  UIStackView* price_tracking_stack_view = GetStackViewFromIdentifier(
      kPriceTrackingStackViewIdentifier, cell_.contentView);
  UIButton* button = base::apple::ObjCCastStrict<UIButton>(
      [price_tracking_stack_view.subviews objectAtIndex:1]);
  EXPECT_EQ(button.accessibilityIdentifier, kPriceTrackingButtonIdentifier);
}

// Tests the presence of UI elements in the Buying Options components when all
// data are available.
TEST_F(PriceInsightsCellTest, TestBuyingOptions) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/true,
      /*has_tracking=*/false,
      /*has_range=*/true,
      /*has_single_range=*/false,
      /*has_history=*/true,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  UILabel* title =
      GetLabelFromIdentifier(kBuyingOptionsTitleIdentifier, cell_.contentView);
  EXPECT_TRUE([title.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PRICE_INSIGHTS_BUYING_OPTIONS_TITLE)]);
  UILabel* subtitle = GetLabelFromIdentifier(kBuyingOptionsSubtitleIdentifier,
                                             cell_.contentView);
  EXPECT_TRUE([subtitle.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PRICE_INSIGHTS_BUYING_OPTIONS_SUBTITLE)]);
}

// Tests the presence of UI elements in the Price History components when only
// Price History data is available.
TEST_F(PriceInsightsCellTest, TestPriceHistoryWithVariantNoTrackingAndNoRange) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/true,
      /*has_tracking=*/false,
      /*has_range=*/false,
      /*has_single_range=*/false,
      /*has_history=*/true,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  UILabel* title =
      GetLabelFromIdentifier(kPriceHistoryTitleIdentifier, cell_.contentView);
  EXPECT_TRUE([title.text isEqualToString:item.title]);
  UILabel* primary_subtitle = GetLabelFromIdentifier(
      kPriceHistoryPrimarySubtitleIdentifier, cell_.contentView);
  EXPECT_TRUE([primary_subtitle.text isEqualToString:item.variants]);
  UILabel* secondary_subtitle = GetLabelFromIdentifier(
      kPriceHistorySecondarySubtitleIdentifier, cell_.contentView);
  EXPECT_TRUE([secondary_subtitle.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PRICE_HISTORY_TITLE_SINGLE_OPTION)]);
}

// Tests the presence of UI elements in the Price History components when only
// Price History data is available, without any variants.
TEST_F(PriceInsightsCellTest,
       TestPriceHistoryWithNoVariantNoTrackingAndNoRange) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/false,
      /*has_tracking=*/false,
      /*has_range=*/false,
      /*has_single_range=*/false,
      /*has_history=*/true,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  UILabel* title =
      GetLabelFromIdentifier(kPriceHistoryTitleIdentifier, cell_.contentView);
  EXPECT_TRUE([title.text isEqualToString:item.title]);
  UILabel* primary_subtitle = GetLabelFromIdentifier(
      kPriceHistoryPrimarySubtitleIdentifier, cell_.contentView);
  EXPECT_TRUE([primary_subtitle.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PRICE_HISTORY_TITLE_SINGLE_OPTION)]);
  UILabel* secondary_subtitle = GetLabelFromIdentifier(
      kPriceHistorySecondarySubtitleIdentifier, cell_.contentView);
  EXPECT_EQ(NULL, secondary_subtitle);
}

// Tests the presence of UI elements in the Price History components when all
// data are available.
TEST_F(PriceInsightsCellTest, TestPriceHistoryWithVariantTrackingAndRange) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/true,
      /*has_tracking=*/true,
      /*has_range=*/true,
      /*has_single_range=*/false,
      /*has_history=*/true,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  UILabel* title =
      GetLabelFromIdentifier(kPriceHistoryTitleIdentifier, cell_.contentView);
  EXPECT_TRUE(
      [title.text isEqualToString:l10n_util::GetNSString(
                                      IDS_PRICE_HISTORY_TITLE_SINGLE_OPTION)]);
  UILabel* primary_subtitle = GetLabelFromIdentifier(
      kPriceHistoryPrimarySubtitleIdentifier, cell_.contentView);
  EXPECT_TRUE([primary_subtitle.text isEqualToString:item.variants]);
  UILabel* secondary_subtitle = GetLabelFromIdentifier(
      kPriceHistorySecondarySubtitleIdentifier, cell_.contentView);
  EXPECT_EQ(NULL, secondary_subtitle);
}

// Tests the presence of UI elements in the Price History components when all
// data are available, without any variants.
TEST_F(PriceInsightsCellTest, TestPriceHistoryWithTrackingRangeAndNoVariant) {
  PriceInsightsItem* item = GetPriceInsights(
      /*has_variant=*/false,
      /*has_tracking=*/true,
      /*has_range=*/true,
      /*has_single_range=*/false,
      /*has_history=*/true,
      /*has_buying_options=*/true);
  [cell_ configureWithItem:item];

  UILabel* title =
      GetLabelFromIdentifier(kPriceHistoryTitleIdentifier, cell_.contentView);
  EXPECT_TRUE(
      [title.text isEqualToString:l10n_util::GetNSString(
                                      IDS_PRICE_HISTORY_TITLE_SINGLE_OPTION)]);
  UILabel* primary_subtitle = GetLabelFromIdentifier(
      kPriceHistoryPrimarySubtitleIdentifier, cell_.contentView);
  EXPECT_EQ(NULL, primary_subtitle);
  UILabel* secondary_subtitle = GetLabelFromIdentifier(
      kPriceHistorySecondarySubtitleIdentifier, cell_.contentView);
  EXPECT_EQ(NULL, secondary_subtitle);
}
