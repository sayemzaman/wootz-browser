// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/address_accessory_controller_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/preferences/autofill/settings_launcher_helper.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// Defines which types to load from the Personal data manager and add as field
// to the address sheet. Order matters.
constexpr FieldType kTypesToInclude[] = {
    FieldType::NAME_FULL,
    FieldType::COMPANY_NAME,
    FieldType::ADDRESS_HOME_LINE1,
    FieldType::ADDRESS_HOME_LINE2,
    FieldType::ADDRESS_HOME_ZIP,
    FieldType::ADDRESS_HOME_CITY,
    FieldType::ADDRESS_HOME_STATE,
    FieldType::ADDRESS_HOME_COUNTRY,
    FieldType::PHONE_HOME_WHOLE_NUMBER,
    FieldType::EMAIL_ADDRESS,
};

void AddProfileInfoAsSelectableField(UserInfo* info,
                                     const AutofillProfile* profile,
                                     FieldType type) {
  std::u16string field = profile->GetRawInfo(type);
  if (type == FieldType::NAME_MIDDLE && field.empty()) {
    field = profile->GetRawInfo(FieldType::NAME_MIDDLE_INITIAL);
  }
  info->add_field(AccessorySheetField(
      /*display_text=*/field, /*text_to_fill=*/field,
      /*a11y_description=*/field, /*id=*/std::string(), /*is_obfuscated=*/false,
      /*selectable=*/true));
}

UserInfo TranslateProfile(const AutofillProfile* profile) {
  UserInfo info;
  for (FieldType field_type : kTypesToInclude) {
    AddProfileInfoAsSelectableField(&info, profile, field_type);
  }
  return info;
}

std::vector<UserInfo> UserInfosForProfiles(
    const std::vector<AutofillProfile*>& profiles) {
  std::vector<UserInfo> infos(profiles.size());
  base::ranges::transform(profiles, infos.begin(), TranslateProfile);
  return infos;
}

std::vector<FooterCommand> CreateManageAddressesFooter() {
  return {FooterCommand(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SHEET_ALL_ADDRESSES_LINK),
      AccessoryAction::MANAGE_ADDRESSES)};
}

}  // namespace

AddressAccessoryControllerImpl::~AddressAccessoryControllerImpl() {
  if (personal_data_manager_)
    personal_data_manager_->RemoveObserver(this);
}

// static
AddressAccessoryController* AddressAccessoryController::GetOrCreate(
    content::WebContents* web_contents) {
  AddressAccessoryControllerImpl::CreateForWebContents(web_contents);
  return AddressAccessoryControllerImpl::FromWebContents(web_contents);
}

void AddressAccessoryControllerImpl::RegisterFillingSourceObserver(
    FillingSourceObserver observer) {
  source_observer_ = std::move(observer);
}

std::optional<autofill::AccessorySheetData>
AddressAccessoryControllerImpl::GetSheetData() const {
  if (!personal_data_manager_) {
    return std::nullopt;
  }
  std::vector<AutofillProfile*> profiles =
      personal_data_manager_->address_data_manager().GetProfilesToSuggest();
  std::u16string title_or_empty_message;
  if (profiles.empty()) {
    title_or_empty_message =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SHEET_EMPTY_MESSAGE);
  }
  return autofill::CreateAccessorySheetData(
      autofill::AccessoryTabType::ADDRESSES, title_or_empty_message,
      UserInfosForProfiles(profiles), CreateManageAddressesFooter());
}

void AddressAccessoryControllerImpl::OnFillingTriggered(
    FieldGlobalId focused_field_id,
    const AccessorySheetField& selection) {
  // Since the data we fill is scoped to the profile and not to a frame, we can
  // fill the focused frame - we basically behave like a keyboard here.
  content::RenderFrameHost* rfh = GetWebContents().GetFocusedFrame();
  if (!rfh)
    return;
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (!driver)
    return;
  driver->browser_events().ApplyFieldAction(
      mojom::FieldActionType::kReplaceAll, mojom::ActionPersistence::kFill,
      focused_field_id, selection.display_text());
}

void AddressAccessoryControllerImpl::OnPasskeySelected(
    const std::vector<uint8_t>& passkey_id) {
  NOTIMPLEMENTED() << "Passkey support not available in address controller.";
}

void AddressAccessoryControllerImpl::OnOptionSelected(
    AccessoryAction selected_action) {
  if (selected_action == AccessoryAction::MANAGE_ADDRESSES) {
    autofill::ShowAutofillProfileSettings(&GetWebContents());
    return;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unhandled selected action: " << static_cast<int>(selected_action);
}

void AddressAccessoryControllerImpl::OnToggleChanged(
    AccessoryAction toggled_action,
    bool enabled) {
  NOTREACHED_IN_MIGRATION()
      << "Unhandled toggled action: " << static_cast<int>(toggled_action);
}

void AddressAccessoryControllerImpl::RefreshSuggestions() {
  TRACE_EVENT0("passwords",
               "AddressAccessoryControllerImpl::RefreshSuggestions");
  if (!personal_data_manager_) {
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetForProfile(
            Profile::FromBrowserContext(GetWebContents().GetBrowserContext()));
    personal_data_manager_->AddObserver(this);
  }
  if (source_observer_) {
    source_observer_.Run(
        this,
        IsFillingSourceAvailable(personal_data_manager_ &&
                                 !personal_data_manager_->address_data_manager()
                                      .GetProfilesToSuggest()
                                      .empty()));
  } else {
    // TODO(crbug.com/40165275): Remove once filling controller pulls this
    // information instead of waiting to get it pushed.
    std::optional<AccessorySheetData> data = GetSheetData();
    DCHECK(data.has_value());
    GetManualFillingController()->RefreshSuggestions(std::move(data).value());
  }
}

base::WeakPtr<AddressAccessoryController>
AddressAccessoryControllerImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AddressAccessoryControllerImpl::OnPersonalDataChanged() {
  RefreshSuggestions();
}

// static
void AddressAccessoryControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(mf_controller);

  web_contents->SetUserData(UserDataKey(),
                            base::WrapUnique(new AddressAccessoryControllerImpl(
                                web_contents, std::move(mf_controller))));
}

AddressAccessoryControllerImpl::AddressAccessoryControllerImpl(
    content::WebContents* web_contents)
    : AddressAccessoryControllerImpl(web_contents, nullptr) {}

// Additional creation functions in unit tests only:
AddressAccessoryControllerImpl::AddressAccessoryControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller)
    : content::WebContentsUserData<AddressAccessoryControllerImpl>(
          *web_contents),
      mf_controller_(std::move(mf_controller)),
      personal_data_manager_(nullptr) {}

base::WeakPtr<ManualFillingController>
AddressAccessoryControllerImpl::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(&GetWebContents());
  DCHECK(mf_controller_);
  return mf_controller_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AddressAccessoryControllerImpl);

}  // namespace autofill
