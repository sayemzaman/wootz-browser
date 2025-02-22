// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_delegate_android_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

namespace {

using test::CreateTestCreditCardFormData;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Ref;
using ::testing::Return;

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(void,
              ScanCreditCard,
              (CreditCardScanCallback callback),
              (override));
  MOCK_METHOD(void, ShowAutofillSettings, (FillingProduct), (override));
  MOCK_METHOD(bool,
              ShowTouchToFillCreditCard,
              (base::WeakPtr<autofill::TouchToFillDelegate> delegate,
               base::span<const CreditCard> cards_to_suggest),
              (override));
  MOCK_METHOD(bool,
              ShowTouchToFillIban,
              (base::WeakPtr<autofill::TouchToFillDelegate> delegate,
               base::span<const Iban> ibanns_to_suggest),
              (override));
  MOCK_METHOD(void, HideTouchToFillCreditCard, (), (override));
  MOCK_METHOD(void,
              HideAutofillSuggestions,
              (SuggestionHidingReason reason),
              (override));

  void ExpectDelegateWeakPtrFromShowInvalidatedOnHideForCards() {
    EXPECT_CALL(*this, ShowTouchToFillCreditCard)
        .WillOnce([this](base::WeakPtr<autofill::TouchToFillDelegate> delegate,
                         base::span<const CreditCard> cards_to_suggest) {
          captured_delegate_ = delegate;
          return true;
        });
    EXPECT_CALL(*this, HideTouchToFillCreditCard).WillOnce([this]() {
      EXPECT_FALSE(captured_delegate_);
    });
  }

  void ExpectDelegateWeakPtrFromShowInvalidatedOnHideForIbans() {
    EXPECT_CALL(*this, ShowTouchToFillIban)
        .WillOnce([this](base::WeakPtr<autofill::TouchToFillDelegate> delegate,
                         base::span<const Iban> ibans_to_suggest) {
          captured_delegate_ = delegate;
          return true;
        });
    EXPECT_CALL(*this, HideTouchToFillCreditCard).WillOnce([this]() {
      EXPECT_FALSE(captured_delegate_);
    });
  }

 private:
  base::WeakPtr<autofill::TouchToFillDelegate> captured_delegate_;
};

class MockBrowserAutofillManager : public TestBrowserAutofillManager {
 public:
  explicit MockBrowserAutofillManager(TestAutofillDriver* driver)
      : TestBrowserAutofillManager(driver) {}
  MockBrowserAutofillManager(const MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(const MockBrowserAutofillManager&) =
      delete;
  ~MockBrowserAutofillManager() override = default;

  MOCK_METHOD(void,
              FillOrPreviewCreditCardForm,
              (mojom::ActionPersistence action_persistence,
               const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const std::u16string& cvc,
               const AutofillTriggerDetails& trigger_details),
              (override));
  MOCK_METHOD(void,
              AuthenticateThenFillCreditCardForm,
              (const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const AutofillTriggerDetails& trigger_details));
  MOCK_METHOD(void,
              DidShowSuggestions,
              (base::span<const SuggestionType> shown_suggestions_types,
               const FormData& form,
               const FormFieldData& field),
              (override));
  MOCK_METHOD(bool, CanShowAutofillUi, (), (const, override));
  MOCK_METHOD(AutofillField*,
              GetAutofillField,
              (const FormData& form, const FormFieldData& field));
};

}  // namespace

class TouchToFillDelegateAndroidImplUnitTest : public testing::Test {
 public:
  TouchToFillDelegateAndroidImplUnitTest() {
    // Some date after in the 2000s because Autofill doesn't allow expiration
    // dates before 2000.
    task_environment_.AdvanceClock(base::Days(365 * 50));
  }

 protected:
  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    autofill_client_.GetPersonalDataManager()->SetPrefService(
        autofill_client_.GetPrefs());
    autofill_driver_ = std::make_unique<TestAutofillDriver>(&autofill_client_);
    browser_autofill_manager_ =
        std::make_unique<NiceMock<MockBrowserAutofillManager>>(
            autofill_driver_.get());

    auto touch_to_fill_delegate =
        std::make_unique<TouchToFillDelegateAndroidImpl>(
            browser_autofill_manager_.get());
    touch_to_fill_delegate_ = touch_to_fill_delegate.get();
    base::WeakPtr<TouchToFillDelegateAndroidImpl> touch_to_fill_delegate_weak =
        touch_to_fill_delegate->GetWeakPtr();
    browser_autofill_manager_->set_touch_to_fill_delegate(
        std::move(touch_to_fill_delegate));

    // Default setup for successful `TryToShowTouchToFill`.
    ON_CALL(*browser_autofill_manager_, CanShowAutofillUi)
        .WillByDefault(Return(true));
    ON_CALL(autofill_client_, ShowTouchToFillCreditCard)
        .WillByDefault(Return(true));
    ON_CALL(autofill_client_, ShowTouchToFillIban).WillByDefault(Return(true));
    // Calling HideTouchToFillCreditCard in production code leads to that
    // OnDismissed gets triggered (HideTouchToFillCreditCard calls view->Hide()
    // on java side, which in its turn triggers onDismissed). Here we mock this
    // call.
    ON_CALL(autofill_client_, HideTouchToFillCreditCard)
        .WillByDefault([delegate = touch_to_fill_delegate_weak]() -> void {
          if (delegate) {
            delegate->OnDismissed(/*dismissed_by_user=*/false);
          }
        });
    autofill::MockFastCheckoutClient* fast_checkout_client =
        static_cast<autofill::MockFastCheckoutClient*>(
            autofill_client_.GetFastCheckoutClient());
    ON_CALL(*fast_checkout_client, IsNotShownYet)
        .WillByDefault(testing::Return(true));
  }

  // Helper method to add the given `card` and create a card form.
  void ConfigureForCreditCards(const CreditCard& card) {
    form_ = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                               /*use_month_type=*/false);
    form_.fields[0].set_is_focusable(true);
    autofill_client_.GetPersonalDataManager()
        ->payments_data_manager()
        .AddCreditCard(card);
  }

  // Helper method to add an IBAN and create an IBAN form.
  std::string ConfigureForIbans() {
    Iban iban;
    iban.set_value(std::u16string(test::kIbanValue16));
    std::string guid = autofill_client_.GetPersonalDataManager()
                           ->test_payments_data_manager()
                           .AddAsLocalIban(std::move(iban));
    form_ = test::CreateTestIbanFormData(/*value=*/"");
    form_.fields[0].set_is_focusable(true);
    return guid;
  }

  void OnFormsSeen() {
    if (!browser_autofill_manager_->FindCachedFormById(form_.global_id())) {
      browser_autofill_manager_->OnFormsSeen({form_}, {});
    }
  }

  void IntendsToShowTouchToFill(bool expected_success) {
    OnFormsSeen();
    EXPECT_EQ(expected_success,
              touch_to_fill_delegate_->IntendsToShowTouchToFill(
                  form_.global_id(), form_.fields[0].global_id(), form_));
  }

  void TryToShowTouchToFill(bool expected_success) {
    EXPECT_CALL(autofill_client_,
                HideAutofillSuggestions(
                    SuggestionHidingReason::kOverlappingWithTouchToFillSurface))
        .Times(expected_success ? 1 : 0);

    OnFormsSeen();
    EXPECT_EQ(expected_success, touch_to_fill_delegate_->TryToShowTouchToFill(
                                    form_, form_.fields[0]));
    EXPECT_EQ(expected_success,
              touch_to_fill_delegate_->IsShowingTouchToFill());
  }

  FormData form_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  std::unique_ptr<MockBrowserAutofillManager> browser_autofill_manager_;
  raw_ptr<TouchToFillDelegateAndroidImpl> touch_to_fill_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

// Params of TouchToFillDelegateAndroidImplPaymentMethodUnitTest:
// -- bool IsCreditCard: Indicates whether the payment method is a credit card
//  or an IBAN.
class TouchToFillDelegateAndroidImplPaymentMethodUnitTest
    : public TouchToFillDelegateAndroidImplUnitTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableLocalIban);
    TouchToFillDelegateAndroidImplUnitTest::SetUp();
    if (IsCreditCard()) {
      ConfigureForCreditCards(test::GetCreditCard());
    } else {
      ConfigureForIbans();
    }
  }

  bool IsCreditCard() const { return GetParam(); }

  std::string GetTriggerOutcomeHistogramName() {
    return IsCreditCard() ? kUmaTouchToFillCreditCardTriggerOutcome
                          : kUmaTouchToFillIbanTriggerOutcome;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
                         testing::Bool());

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsForInvalidForm) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  browser_autofill_manager_->ClearFormStructures();

  EXPECT_EQ(false, touch_to_fill_delegate_->TryToShowTouchToFill(
                       form_, form_.fields[0]));
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfNotSpecificField) {
  form_.fields.insert(form_.fields.begin(),
                      test::CreateTestFormField("Arbitrary", "arbitrary", "",
                                                FormControlType::kInputText));

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       HideTouchToFillDoesNothingIfNotShown) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard).Times(0);
  touch_to_fill_delegate_->HideTouchToFill();
  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       HideTouchToFillHidesIfShown) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard);
  touch_to_fill_delegate_->HideTouchToFill();
  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       ResetHidesTouchToFillIfShown) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard);
  touch_to_fill_delegate_->Reset();
  EXPECT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       ResetAllowsShowingTouchToFillAgain) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->HideTouchToFill();
  TryToShowTouchToFill(/*expected_success=*/false);

  touch_to_fill_delegate_->Reset();
  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       OnDismissSetsTouchToFillToNotShowingState) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->OnDismissed(false);

  EXPECT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), false);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfShownBefore) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true);

  EXPECT_CALL(autofill_client_,
              HideAutofillSuggestions(
                  SuggestionHidingReason::kOverlappingWithTouchToFillSurface))
      .Times(0);
  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfShownCurrently) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_,
              HideAutofillSuggestions(
                  SuggestionHidingReason::kOverlappingWithTouchToFillSurface))
      .Times(0);
  EXPECT_FALSE(
      touch_to_fill_delegate_->TryToShowTouchToFill(form_, form_.fields[0]));
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillPaymentMethodSucceeds) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  EXPECT_CALL(*browser_autofill_manager_, DidShowSuggestions);
  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kShown, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsForPaymentMethodIfWasShown) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->HideTouchToFill();

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectBucketCount(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kShownBefore, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsForPaymentMethodIfFieldIsNotFocusable) {
  form_.fields[0].set_is_focusable(false);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kFieldNotEmptyOrNotFocusable, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsForPaymentMethodIfFieldHasValue) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  form_.fields[0].set_value(u"Initial value");

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kFieldNotEmptyOrNotFocusable, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsForPaymentMethodIfNoPaymentMethodsOnFile) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()->ClearAllLocalData();

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kNoValidPaymentMethods, 1);
}

TEST_P(TouchToFillDelegateAndroidImplPaymentMethodUnitTest,
       TryToShowTouchToFillFailsIfCanNotShowUi) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(*browser_autofill_manager_, CanShowAutofillUi)
      .WillRepeatedly(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      GetTriggerOutcomeHistogramName(),
      TouchToFillPaymentMethodTriggerOutcome::kCannotShowAutofillUi, 1);
}

class TouchToFillDelegateAndroidImplCreditCardUnitTest
    : public TouchToFillDelegateAndroidImplUnitTest {
 public:
  static std::vector<CreditCard> GetCardsToSuggest(
      std::vector<CreditCard*> credit_cards) {
    std::vector<CreditCard> cards_to_suggest;
    cards_to_suggest.reserve(credit_cards.size());
    for (const CreditCard* card : credit_cards) {
      cards_to_suggest.push_back(*card);
    }
    return cards_to_suggest;
  }

 protected:
  void SetUp() override {
    TouchToFillDelegateAndroidImplUnitTest::SetUp();
    ConfigureForCreditCards(test::GetCreditCard());
  }
};

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsForIncompleteForm) {
  // Erase expiration month and expiration year fields.
  ASSERT_EQ(form_.fields[2].name(), u"ccmonth");
  form_.fields.erase(form_.fields.begin() + 2);
  ASSERT_EQ(form_.fields[2].name(), u"ccyear");
  form_.fields.erase(form_.fields.begin() + 2);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);

  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kIncompleteForm, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsForPrefilledCardNumber) {
  // Force the form to be parsed here to test the case, when form values are
  // changed after the form is added to the cache.
  browser_autofill_manager_->OnFormsSeen({form_}, {});
  // Set credit card value.
  // TODO(crbug.com/40900766): Retrieve the card number field by name here.
  ASSERT_EQ(form_.fields[1].name(), u"cardnumber");
  form_.fields[1].set_value(u"411111111111");
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);

  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kFormAlreadyFilled, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillSucceedsForPrefilledYear) {
  // Force the form to be parsed here to test the case, when form values are
  // changed after the form is added to the cache.
  browser_autofill_manager_->OnFormsSeen({form_}, {});
  // Set card expiration year.
  // TODO(crbug.com/40900766): Retrieve the card expiry year field by name here.
  ASSERT_EQ(form_.fields[3].name(), u"ccyear");
  form_.fields[3].set_value(u"2023");
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/true);

  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfFormIsNotSecure) {
  // Simulate non-secure form.
  form_ = test::CreateTestCreditCardFormData(/*is_https=*/false,
                                             /*use_month_type=*/false);

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);

  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kFormOrClientNotSecure, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfClientIsNotSecure) {
  // Simulate non-secure client.
  autofill_client_.set_form_origin(GURL("http://example.com"));

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kFormOrClientNotSecure, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillToleratesFormattingCharacters) {
  form_.fields[0].set_value(u"____-____-____-____");

  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectBucketCount(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfCardIsIncomplete) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard cc_no_number = test::GetCreditCard();
  cc_no_number.SetNumber(u"");
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(cc_no_number);

  TryToShowTouchToFill(/*expected_success=*/false);

  CreditCard cc_no_exp_date = test::GetCreditCard();
  cc_no_exp_date.SetExpirationMonth(0);
  cc_no_exp_date.SetExpirationYear(0);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(cc_no_exp_date);

  TryToShowTouchToFill(/*expected_success=*/false);

  CreditCard cc_no_name = test::GetCreditCard();
  cc_no_name.SetRawInfo(CREDIT_CARD_NAME_FULL, u"");
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(cc_no_name);

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kNoValidPaymentMethods, 3);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfTheOnlyCardIsExpired) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(test::GetExpiredCreditCard());

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kNoValidPaymentMethods, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfCardNumberIsInvalid) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard cc_invalid_number = test::GetCreditCard();
  cc_invalid_number.SetNumber(u"invalid number");
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(cc_invalid_number);

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kNoValidPaymentMethods, 1);

  // But succeeds for existing masked server card with incomplete number.
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(test::GetMaskedServerCard());

  TryToShowTouchToFill(/*expected_success=*/true);
  histogram_tester_.ExpectBucketCount(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfShowFails) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_, ShowTouchToFillCreditCard)
      .WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillFailsIfFastCheckoutWasShown) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  autofill::MockFastCheckoutClient* fast_checkout_client =
      static_cast<autofill::MockFastCheckoutClient*>(
          autofill_client_.GetFastCheckoutClient());
  EXPECT_CALL(*fast_checkout_client, IsNotShownYet).WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
  histogram_tester_.ExpectUniqueSample(
      kUmaTouchToFillCreditCardTriggerOutcome,
      TouchToFillPaymentMethodTriggerOutcome::kFastCheckoutWasShown, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillSucceedsIfAtLestOneCardIsValid) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard expired_card = test::GetExpiredCreditCard();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(expired_card);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_, ShowTouchToFillCreditCard)
      .WillOnce(Return(true));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillShowsExpiredCards) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard expired_card = test::GetExpiredCreditCard();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(expired_card);
  std::vector<CreditCard*> credit_cards =
      autofill_client_.GetPersonalDataManager()
          ->payments_data_manager()
          .GetCreditCardsToSuggest();

  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_,
              ShowTouchToFillCreditCard(
                  _, ElementsAreArray(GetCardsToSuggest(credit_cards))));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillDoesNotShowDisusedExpiredCards) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  CreditCard disused_expired_card = test::GetExpiredCreditCard();
  credit_card.set_use_date(AutofillClock::Now());
  disused_expired_card.set_use_date(AutofillClock::Now() -
                                    kDisusedDataModelTimeDelta * 2);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(disused_expired_card);
  ASSERT_TRUE(credit_card.IsCompleteValidCard());
  ASSERT_FALSE(disused_expired_card.IsCompleteValidCard());
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_,
              ShowTouchToFillCreditCard(_, ElementsAre(credit_card)));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       TryToShowTouchToFillShowsVirtualCardSuggestionsForEnrolledCards) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card =
      autofill::test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_.GetAutofillOptimizationGuide()),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(false));

  // Since the card is enrolled into the virtual cards feature, a virtual card
  // suggestion should be created and added before the real card.
  EXPECT_CALL(
      autofill_client_,
      ShowTouchToFillCreditCard(
          _, ElementsAreArray(
                 {CreditCard::CreateVirtualCard(credit_card), credit_card})));

  TryToShowTouchToFill(/*expected_success=*/true);
}

// Since merchant has opted out of virtual cards, no virtual card suggestion
// is shown for virtual card number enrolled card.
TEST_F(
    TouchToFillDelegateAndroidImplCreditCardUnitTest,
    TryToShowTouchToFillDoesNotShowVirtualCardSuggestionsForOptedOutMerchants) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_.GetAutofillOptimizationGuide()),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(true));
  EXPECT_CALL(autofill_client_,
              ShowTouchToFillCreditCard(_, ElementsAre(credit_card)));
  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       SafelyHideTouchToFillInDtor) {
  autofill_client_.ExpectDelegateWeakPtrFromShowInvalidatedOnHideForCards();
  TryToShowTouchToFill(/*expected_success=*/true);

  browser_autofill_manager_.reset();
}

// Add one IBAN to the PDM and verify that IBAN is not shown for the credit
// card form.
TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       PassTheCreditCardsToTheClient) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .AddAsLocalIban(std::move(iban1));
  CreditCard credit_card1 = autofill::test::GetCreditCard();
  CreditCard credit_card2 = autofill::test::GetCreditCard2();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card1);
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card2);
  std::vector<CreditCard*> credit_cards =
      autofill_client_.GetPersonalDataManager()
          ->payments_data_manager()
          .GetCreditCardsToSuggest();

  EXPECT_CALL(autofill_client_,
              ShowTouchToFillCreditCard(
                  _, ElementsAreArray(GetCardsToSuggest(credit_cards))));

  TryToShowTouchToFill(/*expected_success=*/true);

  browser_autofill_manager_.reset();
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       ScanCreditCardIsCalled) {
  TryToShowTouchToFill(/*expected_success=*/true);
  EXPECT_CALL(autofill_client_, ScanCreditCard);
  touch_to_fill_delegate_->ScanCreditCard();

  CreditCard credit_card = autofill::test::GetCreditCard();
  EXPECT_CALL(*browser_autofill_manager_, FillOrPreviewCreditCardForm);
  touch_to_fill_delegate_->OnCreditCardScanned(credit_card);
  EXPECT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), false);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       ShowCreditCardSettingsIsCalled) {
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_,
              ShowAutofillSettings(testing::Eq(FillingProduct::kCreditCard)));
  touch_to_fill_delegate_->ShowPaymentMethodSettings();

  ASSERT_EQ(touch_to_fill_delegate_->IsShowingTouchToFill(), true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       CardSelectionClosesTheSheet) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard);
  touch_to_fill_delegate_->CreditCardSuggestionSelected(credit_card.guid(),
                                                        false);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       CardSelectionFillsCardForm) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card = autofill::test::GetCreditCard();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(*browser_autofill_manager_, AuthenticateThenFillCreditCardForm);
  touch_to_fill_delegate_->CreditCardSuggestionSelected(credit_card.guid(),
                                                        false);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       VirtualCardSelectionFillsCardForm) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .ClearCreditCards();
  CreditCard credit_card =
      autofill::test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(credit_card);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(*browser_autofill_manager_, AuthenticateThenFillCreditCardForm);
  touch_to_fill_delegate_->CreditCardSuggestionSelected(credit_card.guid(),
                                                        true);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       AutofillUsedAfterTouchToFillDismissal) {
  TryToShowTouchToFill(/*expected_success=*/true);
  touch_to_fill_delegate_->OnDismissed(/*dismissed_by_user=*/true);

  // Simulate that the form was autofilled by other means
  FormStructure submitted_form(form_);
  for (const std::unique_ptr<AutofillField>& field : submitted_form) {
    field->set_is_autofilled(true);
  }

  touch_to_fill_delegate_->LogMetricsAfterSubmission(submitted_form);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.TouchToFill.CreditCard.AutofillUsedAfterTouchToFillDismissal",
      true, 1);
}

TEST_F(TouchToFillDelegateAndroidImplCreditCardUnitTest,
       IsFormPrefilledHandlesNullAutofillField) {
  // `IntendsToShowTouchToFill()` invokes `DryRun()` that checks if form_ is
  // prefilled. `IsFormPrefilled()` calls
  // BrowserAutofillManager::GetAutofillField(). This tests the scenario where
  // `GetAutofillField()` returns a nullptr does not crash.
  ON_CALL(*browser_autofill_manager_, GetAutofillField(_, _))
      .WillByDefault(Return(nullptr));
  IntendsToShowTouchToFill(/*expected_success=*/true);
}

class TouchToFillDelegateAndroidImplIbanUnitTest
    : public TouchToFillDelegateAndroidImplUnitTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableLocalIban);
    TouchToFillDelegateAndroidImplUnitTest::SetUp();
    ConfigureForIbans();
  }
};

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       TryToShowTouchToFillFailsIfShowFails) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_, ShowTouchToFillIban).WillOnce(Return(false));

  TryToShowTouchToFill(/*expected_success=*/false);
}

// Add one valid credit card to PDM and verify that credit card is not shown in
// IBAN form.
TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest, PassTheIbansToTheClient) {
  autofill_client_.GetPersonalDataManager()->ClearAllLocalData();
  autofill_client_.GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(autofill::test::GetCreditCard());
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .AddAsLocalIban(std::move(iban1));
  Iban iban2;
  iban2.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_2)));
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .AddAsLocalIban(std::move(iban2));
  std::vector<Iban> ibans = autofill_client_.GetPersonalDataManager()
                                ->test_payments_data_manager()
                                .GetOrderedIbansToSuggest();

  EXPECT_CALL(autofill_client_,
              ShowTouchToFillIban(_, ElementsAreArray(ibans)));

  TryToShowTouchToFill(/*expected_success=*/true);

  browser_autofill_manager_.reset();
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       TryToShowTouchToFillSucceeds) {
  ASSERT_FALSE(touch_to_fill_delegate_->IsShowingTouchToFill());
  EXPECT_CALL(autofill_client_, ShowTouchToFillIban).WillOnce(Return(true));

  TryToShowTouchToFill(/*expected_success=*/true);
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       SafelyHideTouchToFillInDtor) {
  autofill_client_.ExpectDelegateWeakPtrFromShowInvalidatedOnHideForIbans();
  TryToShowTouchToFill(/*expected_success=*/true);

  browser_autofill_manager_.reset();
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       IbanSelectionClosesTheSheet) {
  std::string guid = ConfigureForIbans();
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(autofill_client_, HideTouchToFillCreditCard);
  touch_to_fill_delegate_->IbanSuggestionSelected(Iban::Guid(guid));
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       LocalIbanSelectionFillsIbanForm) {
  std::string guid = ConfigureForIbans();
  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(*(autofill_client_.GetMockIbanAccessManager()), FetchValue);
  touch_to_fill_delegate_->IbanSuggestionSelected(Iban::Guid(guid));
}

TEST_F(TouchToFillDelegateAndroidImplIbanUnitTest,
       ServerIbanSelectionFillsIbanForm) {
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .SetSyncingForTest(true);
  std::string guid = ConfigureForIbans();
  // Add a server IBAN with a different instrument_id and verify `FetchValue`
  // is not triggered.
  long instrument_id = 123245678L;
  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(instrument_id));
  autofill_client_.GetPersonalDataManager()
      ->test_payments_data_manager()
      .AddServerIban(server_iban);

  TryToShowTouchToFill(/*expected_success=*/true);

  EXPECT_CALL(*(autofill_client_.GetMockIbanAccessManager()), FetchValue);
  touch_to_fill_delegate_->IbanSuggestionSelected(
      Iban::InstrumentId(instrument_id));
}

}  // namespace autofill
