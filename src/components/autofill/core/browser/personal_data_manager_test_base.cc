// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "base/task/single_thread_task_runner.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

namespace {

const char kPrimaryAccountEmail[] = "syncuser@example.com";
const char kSyncTransportAccountEmail[] = "transport@example.com";

}  // anonymous namespace

PersonalDataManagerTestBase::PersonalDataManagerTestBase() = default;

PersonalDataManagerTestBase::~PersonalDataManagerTestBase() = default;

void PersonalDataManagerTestBase::SetUpTest() {
  OSCryptMocker::SetUp();
  prefs_ = test::PrefServiceForTesting();
  base::FilePath path(WebDatabase::kInMemoryPath);
  profile_web_database_ = new WebDatabaseService(
      path, base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  profile_web_database_->AddTable(std::make_unique<AddressAutofillTable>());
  // Hacky: hold onto a pointer but pass ownership.
  profile_autofill_table_ = new PaymentsAutofillTable;
  profile_web_database_->AddTable(
      std::unique_ptr<WebDatabaseTable>(profile_autofill_table_));
  profile_web_database_->LoadDatabase();
  profile_database_service_ = new AutofillWebDataService(
      profile_web_database_, base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  profile_database_service_->Init(base::NullCallback());

  account_web_database_ =
      new WebDatabaseService(base::FilePath(WebDatabase::kInMemoryPath),
                             base::SingleThreadTaskRunner::GetCurrentDefault(),
                             base::SingleThreadTaskRunner::GetCurrentDefault());
  account_autofill_table_ = new PaymentsAutofillTable;
  account_web_database_->AddTable(
      std::unique_ptr<WebDatabaseTable>(account_autofill_table_));
  account_web_database_->LoadDatabase();
  account_database_service_ = new AutofillWebDataService(
      account_web_database_, base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  account_database_service_->Init(base::NullCallback());

  strike_database_ = std::make_unique<TestInMemoryStrikeDatabase>();

  test::DisableSystemServices(prefs_.get());
}

void PersonalDataManagerTestBase::TearDownTest() {
  // Order of destruction is important as BrowserAutofillManager relies on
  // PersonalDataManager to be around when it gets destroyed.
  test::ReenableSystemServices();
  OSCryptMocker::TearDown();
}

void PersonalDataManagerTestBase::MakePrimaryAccountAvailable(
    bool use_sync_transport_mode) {
  std::string email = use_sync_transport_mode ? kSyncTransportAccountEmail
                                              : kPrimaryAccountEmail;
  // Set the account in both IdentityManager and SyncService.
  CoreAccountInfo account_info;
  signin::ConsentLevel consent_level = use_sync_transport_mode
                                           ? signin::ConsentLevel::kSignin
                                           : signin::ConsentLevel::kSync;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  identity_test_env_.ClearPrimaryAccount();
  account_info =
      identity_test_env_.MakePrimaryAccountAvailable(email, consent_level);
#else
  // In ChromeOS-Ash, clearing/resetting the primary account is not supported.
  // So if an account already exists, reuse it (and make sure it matches).
  if (identity_test_env_.identity_manager()->HasPrimaryAccount(consent_level)) {
    account_info = identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
        consent_level);
    ASSERT_EQ(account_info.email, email);
  } else {
    account_info =
        identity_test_env_.MakePrimaryAccountAvailable(email, consent_level);
  }
#endif
  sync_service_.SetAccountInfo(account_info);
  sync_service_.SetHasSyncConsent(!use_sync_transport_mode);
}

std::unique_ptr<PersonalDataManager>
PersonalDataManagerTestBase::InitPersonalDataManager(
    bool use_sync_transport_mode) {
  MakePrimaryAccountAvailable(use_sync_transport_mode);
  auto personal_data = std::make_unique<PersonalDataManager>(
      profile_database_service_, account_database_service_, prefs_.get(),
      prefs_.get(), identity_test_env_.identity_manager(),
      /*history_service=*/nullptr, &sync_service_, strike_database_.get(),
      /*image_fetcher=*/nullptr, /*shared_storage_handler=*/nullptr, "en-US",
      "US");
  PersonalDataChangedWaiter(*personal_data).Wait();
  return personal_data;
}

}  // namespace autofill
