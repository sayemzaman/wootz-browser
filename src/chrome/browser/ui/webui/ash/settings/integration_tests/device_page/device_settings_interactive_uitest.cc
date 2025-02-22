// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input-event-codes.h>
#include <memory>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

const ui::TouchpadDevice kSampleTouchpadInternal(1,
                                                 ui::INPUT_DEVICE_INTERNAL,
                                                 "touchpad");

const ui::InputDevice kMouse(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse");

const ui::InputDevice kFiveKeyMouse(/*id=*/15,
                                    ui::INPUT_DEVICE_USB,
                                    "kFiveKeyMouse",
                                    /*phys=*/"",
                                    /*sys_path=*/base::FilePath(),
                                    /*vendor=*/0x1532,
                                    /*product=*/0x0090,
                                    /*version=*/0x0001);

inline constexpr int kDeviceId1 = 5;
constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayout1Tag[] = "1";

class FakeDeviceManager {
 public:
  FakeDeviceManager() = default;
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() = default;

  // Add a fake keyboard to DeviceDataManagerTestApi and provide layout info to
  // fake udev.
  void AddFakeInternalKeyboard() {
    ui::KeyboardDevice fake_keyboard(
        /*id=*/kDeviceId1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
        /*name=*/"Keyboard1");
    fake_keyboard.sys_path = base::FilePath("path1");

    fake_keyboard_devices_.push_back(fake_keyboard);
    ui::KeyboardCapability::KeyboardInfo keyboard_info;
    keyboard_info.device_type =
        ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
    keyboard_info.top_row_layout =
        ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1;

    keyboard_info.top_row_action_keys = std::vector<ui::TopRowActionKey>(
        std::begin(ui::kLayout1TopRowActionKeys),
        std::end(ui::kLayout1TopRowActionKeys));

    Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
        fake_keyboard, std::move(keyboard_info));
    // Calling RunUntilIdle() here is necessary before setting the keyboard
    // devices to prevent the callback from evdev thread to overwrite whatever
    // we sethere below. See
    // `InputDeviceFactoryEvdevProxy::OnStartupScanComplete()`.
    base::RunLoop().RunUntilIdle();
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    std::map<std::string, std::string> sysfs_properties;
    std::map<std::string, std::string> sysfs_attributes;
    sysfs_properties[kKbdTopRowPropertyName] = kKbdTopRowLayout1Tag;
    fake_udev_.AddFakeDevice(fake_keyboard.name, fake_keyboard.sys_path.value(),
                             /*subsystem=*/"input", /*devnode=*/std::nullopt,
                             /*devtype=*/std::nullopt,
                             std::move(sysfs_attributes),
                             std::move(sysfs_properties));
  }

 private:
  testing::FakeUdevLoader fake_udev_;
  std::vector<ui::KeyboardDevice> fake_keyboard_devices_;
};

class DeviceSettingsInteractiveUiTest : public InteractiveAshTest {
 public:
  DeviceSettingsInteractiveUiTest() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);
    webcontents_id_ = kOsSettingsWebContentsId;

    feature_list_.InitWithFeatures(
        {features::kInputDeviceSettingsSplit,
         features::kAltClickAndSixPackCustomization,
         features::kPeripheralCustomization,
         features::kEnableKeyboardBacklightControlInSettings,
         ::features::kSupportF11AndF12KeyShortcuts},
        {});
  }

  DeviceSettingsInteractiveUiTest(const DeviceSettingsInteractiveUiTest&) =
      delete;
  DeviceSettingsInteractiveUiTest& operator=(
      const DeviceSettingsInteractiveUiTest&) = delete;

  ~DeviceSettingsInteractiveUiTest() override = default;

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();

    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
  }

  // Query to pierce through Shadow DOM to find the keyboard.
  const DeepQuery kKeyboardNameQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-keyboard",
      "settings-per-device-keyboard-subsection",
      "h2#keyboardName",
  };

  const DeepQuery kKeyboardRowQuery{
      "os-settings-ui",       "os-settings-main",      "main-page-container",
      "settings-device-page", "#perDeviceKeyboardRow",
  };

  // Query to pierce through Shadow DOM to find the Settings search box.
  const DeepQuery kSearchboxQuery{
      "os-settings-ui", "settings-toolbar", "#searchBox",
      "#search",        "#searchInput",
  };

  // Query to pierce through Shadow DOM to find the Keyboard header.
  const DeepQuery kCustomizeKeyboardKeysInternalQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-keyboard",
      "settings-per-device-keyboard-subsection",
      ".remap-keyboard-keys-row-internal",
  };

  const DeepQuery kMouseRowQuery{
      "os-settings-ui",       "os-settings-main",   "main-page-container",
      "settings-device-page", "#perDeviceMouseRow",
  };

  auto WaitForSearchboxContainsText(const std::string& text) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTextFound);
    StateChange change;
    change.event = kTextFound;
    change.where = kSearchboxQuery;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    const std::string value_check_function =
        base::StringPrintf("(e) => { return e.value == '%s';}", text.c_str());
    change.test_function = value_check_function;
    return WaitForStateChange(webcontents_id_, change);
  }

  auto LaunchSettingsApp(const ui::ElementIdentifier& element_id,
                         const std::string& subpage) {
    return Steps(
        Log(std::format("Open OS Settings to {0}", subpage)),
        InstrumentNextTab(element_id, AnyBrowser()), Do([&]() {
          chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
              GetActiveUserProfile(), subpage);
        }),
        WaitForShow(element_id),
        Log(std::format("Waiting for OS Settings {0} page to load", subpage)),

        Log("Waiting for OS settings audio settings page to load"),
        WaitForWebContentsReady(element_id, chrome::GetOSSettingsUrl(subpage)));
  }

  // Enters lower-case text into the focused html input element.
  auto EnterLowerCaseText(const std::string& text) {
    return Do([&]() {
      for (char c : text) {
        ui::test::EventGenerator(Shell::GetPrimaryRootWindow())
            .PressKey(static_cast<ui::KeyboardCode>(ui::VKEY_A + (c - 'a')),
                      ui::EF_NONE, kDeviceId1);
      }
    });
  }

  auto SendKeyPressEvent(ui::KeyboardCode key, int modifier = ui::EF_NONE) {
    return Do([key, modifier]() {
      ui::test::EventGenerator(Shell::GetPrimaryRootWindow())
          .PressKeyAndModifierKeys(key, modifier, kDeviceId1);
    });
  }

  auto SetupInternalKeyboard() {
    return Do([&]() { fake_keyboard_manager_->AddFakeInternalKeyboard(); });
  }

  void SetMouseDevices(const std::vector<ui::InputDevice>& mice) {
    base::RunLoop().RunUntilIdle();
    ui::DeviceDataManagerTestApi().SetMouseDevices(mice);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  }

  void SetTouchpadDevices(const std::vector<ui::TouchpadDevice>& touchpads) {
    base::RunLoop().RunUntilIdle();
    ui::DeviceDataManagerTestApi().SetTouchpadDevices(touchpads);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  }

  void SetPointingStickDevices(
      const std::vector<ui::InputDevice>& pointing_sticks) {
    base::RunLoop().RunUntilIdle();
    ui::DeviceDataManagerTestApi().SetPointingStickDevices(pointing_sticks);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  }

  auto SendKeyPressAndReleaseEvent(ui::KeyboardCode key, int modifier) {
    return Do([key, modifier]() {
      ui::test::EventGenerator(Shell::GetPrimaryRootWindow())
          .PressAndReleaseKeyAndModifierKeys(key, modifier, kDeviceId1);
    });
  }

  auto WaitForDropdownContainsValue(const DeepQuery& query, int value) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kOptionFound);
    StateChange change;
    change.event = kOptionFound;
    change.where = query;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    change.test_function =
        base::StringPrintf("e => e.selectedIndex === %i", value);
    return WaitForStateChange(webcontents_id_, change);
  }

 protected:
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
  base::test::ScopedFeatureList feature_list_;
  ui::ElementIdentifier webcontents_id_;
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest, AddNewKeyboard) {
  RunTestSequence(SetupInternalKeyboard(),
                  LaunchSettingsApp(
                      webcontents_id_,
                      chromeos::settings::mojom::kPerDeviceKeyboardSubpagePath),
                  Log("Waiting for keyboard to exist"),
                  WaitForElementExists(webcontents_id_, kKeyboardNameQuery),
                  CheckJsResultAt(webcontents_id_, kKeyboardNameQuery,
                                  "el => el.innerText", "Built-in Keyboard"),
                  Log("Test complete"));
}

IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest, OpenTouchpadSubpage) {
  // Query to pierce through Shadow DOM to find the touchpad row.
  const DeepQuery kTouchpadRowQuery{
      "os-settings-ui",       "os-settings-main",      "main-page-container",
      "settings-device-page", "#perDeviceTouchpadRow",
  };

  // Query to pierce through Shadow DOM to find the touchpad header.
  const DeepQuery kTouchpadNameQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-touchpad",
      "settings-per-device-touchpad-subsection",
      "h2#touchpadName",
  };

  SetTouchpadDevices({kSampleTouchpadInternal});
  RunTestSequence(
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      WaitForElementExists(webcontents_id_, kTouchpadRowQuery),
      ClickElement(webcontents_id_, kTouchpadRowQuery),
      WaitForElementTextContains(webcontents_id_, kTouchpadNameQuery,
                                 "Built-in Touchpad"));
}

IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest, OpenKeyboardSubpage) {
  RunTestSequence(
      SetupInternalKeyboard(),
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      WaitForElementExists(webcontents_id_, kKeyboardRowQuery),
      ClickElement(webcontents_id_, kKeyboardRowQuery),
      WaitForElementTextContains(webcontents_id_, kKeyboardNameQuery,
                                 "Built-in Keyboard"));
}

IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest, AddNewMouse) {
  // Query to pierce through Shadow DOM to find the mouse row.
  const DeepQuery kMouseRowQuery{
      "os-settings-ui",       "os-settings-main",   "main-page-container",
      "settings-device-page", "#perDeviceMouseRow",
  };

  // Query to pierce through Shadow DOM to find the mouse header.
  const DeepQuery kMouseNameQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "h2#mouseName",
  };

  SetMouseDevices({ui::InputDevice(
      /*id=*/3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  RunTestSequence(
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kPerDeviceMouseSubpagePath),
      Log("Waiting for Mouse to exist"),
      WaitForElementExists(webcontents_id_, kMouseNameQuery),
      WaitForElementTextContains(webcontents_id_, kMouseNameQuery, "mouse"));
}

IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest, SixPackKeys) {
  const DeepQuery kDeleteDropdownQuery{
      "os-settings-ui",      "os-settings-main",
      "main-page-container", "settings-device-page",
      "#remap-keys",         "keyboard-six-pack-key-row:nth-child(1)",
      "#keyDropdown",        "#dropdownMenu",
  };

  RunTestSequence(
      Log("Adding a fake internal keyboard"), SetupInternalKeyboard(),
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      WaitForElementExists(webcontents_id_, kKeyboardRowQuery),
      ClickElement(webcontents_id_, kKeyboardRowQuery),
      WaitForElementTextContains(webcontents_id_, kKeyboardNameQuery,
                                 "Built-in Keyboard"),
      ClickElement(webcontents_id_, kCustomizeKeyboardKeysInternalQuery),
      Log("Remapping the 'Delete' action to 'Alt + Backspace'"),
      ExecuteJsAt(webcontents_id_, kDeleteDropdownQuery,
                  "(el) => {el.selectedIndex = 0; el.dispatchEvent(new "
                  "Event('change'));}"),
      ExecuteJsAt(webcontents_id_, kSearchboxQuery,
                  "(el) => { el.focus(); el.select(); }"),
      Log("Entering 'redo' into the Settings search box"),
      EnterLowerCaseText("redo"), WaitForSearchboxContainsText("redo"),
      Log("Pressing the 'Left' key"),
      SendKeyPressEvent(ui::KeyboardCode::VKEY_LEFT),
      Log("Pressing 'Alt + Backspace' to generate the 'Delete' action"),
      SendKeyPressEvent(ui::KeyboardCode::VKEY_MENU),
      SendKeyPressEvent(ui::KeyboardCode::VKEY_BACK, ui::EF_ALT_DOWN),
      Log("Verifying that the 'Delete' action was performed and the search "
          "box now contains the text 'red'"),
      WaitForSearchboxContainsText("red"));
}

IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest, TopRow) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsExploreAppWebbContentsId);

  // Query to pierce through Shadow DOM to find the
  // 'Treat top-row keys as function keys' toggle.
  const DeepQuery kTopRowAreFkeysToggleQuery{{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-keyboard",
      "settings-per-device-keyboard-subsection",
      "settings-toggle-button#internalTopRowAreFunctionKeysButton",
  }};

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTopRowAreFkeysEvent);
  StateChange top_row_are_fkeys;
  top_row_are_fkeys.type = StateChange::Type::kExistsAndConditionTrue;
  top_row_are_fkeys.event = kTopRowAreFkeysEvent;
  top_row_are_fkeys.where = kTopRowAreFkeysToggleQuery;
  top_row_are_fkeys.test_function = "btn => btn.checked";

  RunTestSequence(
      Log("Adding a fake internal keyboard"), SetupInternalKeyboard(),
      LaunchSettingsApp(
          webcontents_id_,
          chromeos::settings::mojom::kPerDeviceKeyboardSubpagePath),
      Log("Enabling 'Treat top-row keys as function keys' setting"),
      ClickElement(webcontents_id_, kTopRowAreFkeysToggleQuery),
      WaitForStateChange(webcontents_id_, top_row_are_fkeys),
      Log("Verifying that the top row back button opens the Explore app"),
      SendKeyPressEvent(ui::KeyboardCode::VKEY_F1),
      InstrumentNextTab(kOsExploreAppWebbContentsId, AnyBrowser()),
      WaitForShow(kOsExploreAppWebbContentsId));
}

IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest, TrackpointEnabled) {
  const ui::InputDevice kSamplePointingStickInternal(
      2, ui::INPUT_DEVICE_INTERNAL, "kSamplePointingStickInternal");

  // Query to pierce through Shadow DOM to find the pointing stick row.
  const DeepQuery kPointingStickRowQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "#perDevicePointingStickRow",
  };

  // Query to pierce through Shadow DOM to find the pointing stick header.
  const DeepQuery kPointingStickNameQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-pointing-stick",
      "settings-per-device-pointing-stick-subsection",
      "h2#pointingStickName",
  };
  SetPointingStickDevices({kSamplePointingStickInternal});
  RunTestSequence(
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      WaitForElementExists(webcontents_id_, kPointingStickRowQuery),
      ClickElement(webcontents_id_, kPointingStickRowQuery),
      WaitForElementTextContains(webcontents_id_, kPointingStickNameQuery,
                                 "Built-in TrackPoint"));
}

class DeviceSettingsSwapPrimaryMouseButtonInteractiveUiTest
    : public DeviceSettingsInteractiveUiTest {
 public:
  DeviceSettingsSwapPrimaryMouseButtonInteractiveUiTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures({features::kInputDeviceSettingsSplit},
                                   {features::kPeripheralCustomization});
  }
  // Query to pierce through Shadow DOM to find the mouse row.
  const DeepQuery kMouseRowQuery{
      "os-settings-ui",       "os-settings-main",   "main-page-container",
      "settings-device-page", "#perDeviceMouseRow",
  };

  const DeepQuery kMouseSwapButtonDropdownQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "#mouseSwapButtonDropdown",
      "#dropdownMenu",
  };

  const DeepQuery kCursorAcceleartorToggleQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "#mouseAcceleration",
      "#control",
  };
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsSwapPrimaryMouseButtonInteractiveUiTest,
                       SwapPrimaryMouseButton) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCursorAccelerationToggleEnabledEvent);
  SetMouseDevices({kMouse});
  StateChange cursor_acceleration_toggle_enabled;
  cursor_acceleration_toggle_enabled.type =
      StateChange::Type::kExistsAndConditionTrue;
  cursor_acceleration_toggle_enabled.event =
      kCursorAccelerationToggleEnabledEvent;
  cursor_acceleration_toggle_enabled.where = kCursorAcceleartorToggleQuery;
  cursor_acceleration_toggle_enabled.test_function = "el => !el.disabled";

  RunTestSequence(
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      Log("Waiting for per device mouse row to be visible"),
      WaitForElementExists(webcontents_id_, kMouseRowQuery),
      ClickElement(webcontents_id_, kMouseRowQuery),
      Log("Waiting for swap primary mouse toggle to be visible"),
      WaitForElementExists(webcontents_id_, kMouseSwapButtonDropdownQuery),
      Log("Selecting 'Right button' from the dropdown menu"),
      ExecuteJsAt(webcontents_id_, kMouseSwapButtonDropdownQuery,
                  "(el) => {el.selectedIndex = 1; el.dispatchEvent(new "
                  "Event('change'));}"),
      Log("Verifying that right clicking behavior has changed"),
      MoveMouseTo(webcontents_id_, kCursorAcceleartorToggleQuery),
      ClickMouse(ui_controls::RIGHT),
      WaitForStateChange(webcontents_id_, cursor_acceleration_toggle_enabled));
}

IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest, AddNewTouchpad) {
  // Query to pierce through Shadow DOM to find the touchpad row.
  const DeepQuery kTouchpadRowQuery{
      "os-settings-ui",       "os-settings-main",      "main-page-container",
      "settings-device-page", "#perDeviceTouchpadRow",
  };

  // Query to pierce through Shadow DOM to find the touchpad header.
  const DeepQuery kTouchpadNameQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-touchpad",
      "settings-per-device-touchpad-subsection",
      "h2#touchpadName",
  };

  SetTouchpadDevices({kSampleTouchpadInternal});
  RunTestSequence(
      LaunchSettingsApp(
          webcontents_id_,
          chromeos::settings::mojom::kPerDeviceTouchpadSubpagePath),
      Log("Waiting for Touchpad to exist"),
      WaitForElementExists(webcontents_id_, kTouchpadNameQuery),
      WaitForElementTextContains(webcontents_id_, kTouchpadNameQuery,
                                 "Built-in Touchpad"));
}

IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest,
                       KeyboardModifierRemapping) {
  const DeepQuery kCtrlDropdownQuery{
      "os-settings-ui",       "os-settings-main", "main-page-container",
      "settings-device-page", "#remap-keys",      "#ctrlKey",
      "#keyDropdown",         "#dropdownMenu",
  };

  RunTestSequence(
      Log("Adding a fake internal keyboard"), SetupInternalKeyboard(),
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      WaitForElementExists(webcontents_id_, kKeyboardRowQuery),
      ClickElement(webcontents_id_, kKeyboardRowQuery),
      WaitForElementTextContains(webcontents_id_, kKeyboardNameQuery,
                                 "Built-in Keyboard"),
      ClickElement(webcontents_id_, kCustomizeKeyboardKeysInternalQuery),
      Log("Remapping the 'Ctrl' key to 'Backspace'"),
      ExecuteJsAt(webcontents_id_, kCtrlDropdownQuery,
                  "(el) => {el.selectedIndex = 5; el.dispatchEvent(new "
                  "Event('change'));}"),
      ExecuteJsAt(webcontents_id_, kSearchboxQuery,
                  "(el) => { el.focus(); el.select(); }"),
      Log("Entering 'redo' into the Settings search box"),
      EnterLowerCaseText("redo"), WaitForSearchboxContainsText("redo"),
      Log("Pressing the 'Ctrl' key"),
      SendKeyPressEvent(ui::KeyboardCode::VKEY_CONTROL),
      Log("Verifying that the 'Backspace' action was performed and the search "
          "box now contains the text 'red'"),
      WaitForSearchboxContainsText("red"));
}

// Disabled for crbug.com/325543031.
IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest,
                       DISABLED_MouseKeyCombination) {
  const DeepQuery kCustomizeMouseButtonsRowQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "#customizeMouseButtons",
      "#icon",
  };

  const DeepQuery kCustomizeMouseButtonsHelpSectionQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-customize-mouse-buttons-subpage",
      "#helpSection",
  };

  const DeepQuery kCustomizeButtonsSubsectionQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "#customizeMouseButtonsRow > settings-customize-mouse-buttons-subpage",
      "#buttonsSection > customize-buttons-subsection",
  };

  const DeepQuery kRemappingActionDropdownQuery =
      kCustomizeButtonsSubsectionQuery +
      "div > customize-button-row:nth-child(1)" + "#remappingActionDropdown";

  const DeepQuery kKeyCombinationSaveQuery = kCustomizeButtonsSubsectionQuery +
                                             "key-combination-input-dialog" +
                                             "#saveButton";

  SetMouseDevices({kFiveKeyMouse});

  RunTestSequence(
      SetupInternalKeyboard(),
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kPerDeviceMouseSubpagePath),
      Log("Clicking customize mouse buttons row"),
      ClickElement(webcontents_id_, kCustomizeMouseButtonsRowQuery),
      Log("Waiting for customize mouse buttons page"),
      WaitForElementExists(webcontents_id_,
                           kCustomizeMouseButtonsHelpSectionQuery),
      Log("Registering a new button for the mouse"),

      Do([&]() {
        ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
        generator.PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE, kFiveKeyMouse.id);
      }),
      Log("Opening Remapping Action Dropdown"),
      ClickElement(webcontents_id_, kRemappingActionDropdownQuery),
      Log("Opening Key Combination dialog"), Do([&]() {
        ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
        // Select the 15th option in the dropdown menu (key combination)
        for (int i = 0; i < 15; i++) {
          generator.PressAndReleaseKey(ui::VKEY_DOWN, ui::EF_NONE, kDeviceId1);
        }
        generator.PressAndReleaseKey(ui::VKEY_RETURN, ui::EF_NONE, kDeviceId1);
      }),
      Log("Waiting for Key Combination dialog"),
      WaitForElementExists(webcontents_id_, kKeyCombinationSaveQuery),
      Log("Typing Key Combination"), Do([&]() {
        ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
        generator.PressAndReleaseKeyAndModifierKeys(
            ui::VKEY_C, ui::EF_COMMAND_DOWN, kDeviceId1);
      }),
      Log("Clicking Save Button"),
      ClickElement(webcontents_id_, kKeyCombinationSaveQuery),
      Log("Navigate back one page"),
      SendAccelerator(webcontents_id_, {ui::VKEY_BROWSER_BACK, ui::EF_NONE}),
      WaitForElementExists(webcontents_id_, kCustomizeMouseButtonsRowQuery),

      Log("Check to make sure calendar is already not visible"),
      EnsureNotPresent(kCalendarViewElementId),
      Log("Activating remapped button to open calendar with Search + C"),
      Do([&]() {
        ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
        generator.PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE, kFiveKeyMouse.id);
      }),
      WaitForShow(kCalendarViewElementId),
      Log("Calendar opened with mouse button"), Do([&]() {
        ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
        generator.PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE, kFiveKeyMouse.id);
      }),
      WaitForHide(kCalendarViewElementId),
      Log("Calendar closed with mouse button"));
}

// Disabled for crbug.com/325543031.
IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest,
                       DISABLED_MouseButtonRenaming) {
  const DeepQuery kCustomizeMouseButtonsRowQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "#customizeMouseButtons",
      "#icon",
  };

  const DeepQuery kCustomizeButtonsSubsectionQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "#customizeMouseButtonsRow > settings-customize-mouse-buttons-subpage",
      "#buttonsSection > customize-buttons-subsection",
  };

  const DeepQuery kMiddleButtonEditButtonQuery =
      kCustomizeButtonsSubsectionQuery +
      "div > customize-button-row:nth-child(1)" +
      "#container > div.edit-icon-container > cr-icon-button";

  const DeepQuery kSaveButtonQuery =
      kCustomizeButtonsSubsectionQuery + "#saveButton";

  const DeepQuery kCustomizeableButtonNameQuery =
      kCustomizeButtonsSubsectionQuery +
      "div > customize-button-row:nth-child(1)" + "#buttonLabel";

  SetMouseDevices({kFiveKeyMouse});
  // Used to relaunch the settings app after the customizable mouse button
  // has been edited.
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewSettingsAppWebContentsId);

  RunTestSequence(
      SetupInternalKeyboard(),
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kPerDeviceMouseSubpagePath),
      Log("Clicking customize mouse buttons row"),
      ClickElement(webcontents_id_, kCustomizeMouseButtonsRowQuery),
      Log("Clicking edit icon for mouse 'Middle Button'"),
      ClickElement(webcontents_id_, kMiddleButtonEditButtonQuery),
      Log("Clearing existing mouse button name"),
      SendKeyPressEvent(ui::KeyboardCode::VKEY_A, ui::EF_CONTROL_DOWN),
      SendKeyPressEvent(ui::KeyboardCode::VKEY_BACK),
      Log("Renaming mouse button to 'custom'"), EnterLowerCaseText("custom"),
      ClickElement(webcontents_id_, kSaveButtonQuery),
      Log("Verifying that the custom mouse button has been renamed to "
          "'custom'"),
      WaitForElementTextContains(webcontents_id_, kCustomizeableButtonNameQuery,
                                 "custom"),
      Log("Closing the Settings app"),
      SendAccelerator(webcontents_id_,
                      {ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}),
      WaitForHide(webcontents_id_, /*transition_only_on_event=*/true),
      LaunchSettingsApp(kNewSettingsAppWebContentsId,
                        chromeos::settings::mojom::kPerDeviceMouseSubpagePath),
      ClickElement(kNewSettingsAppWebContentsId,
                   kCustomizeMouseButtonsRowQuery),
      Log("Confirming the updated mouse button name is saved correctly"),
      WaitForElementTextContains(kNewSettingsAppWebContentsId,
                                 kCustomizeableButtonNameQuery, "custom")

  );
}

IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest,
                       MouseScrollAcceleration) {
  const DeepQuery kScrollingSpeedSliderQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "#mouseScrollSpeedSlider",
  };

  const DeepQuery kControlledScrollingButtonQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "#mouseControlledScrolling",
  };

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kScrollingSpeedSliderDisabledEvent);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kScrollingSpeedSliderEnabledEvent);
  SetMouseDevices({kMouse});

  StateChange scrolling_speed_slider_disabled;
  scrolling_speed_slider_disabled.type =
      StateChange::Type::kExistsAndConditionTrue;
  scrolling_speed_slider_disabled.event = kScrollingSpeedSliderDisabledEvent;
  scrolling_speed_slider_disabled.where = kScrollingSpeedSliderQuery;
  scrolling_speed_slider_disabled.test_function = "el => el.disabled";

  StateChange scrolling_speed_slider_enabled;
  scrolling_speed_slider_enabled.type =
      StateChange::Type::kExistsAndConditionTrue;
  scrolling_speed_slider_enabled.event = kScrollingSpeedSliderEnabledEvent;
  scrolling_speed_slider_enabled.where = kScrollingSpeedSliderQuery;
  scrolling_speed_slider_enabled.test_function = "el => !el.disabled";

  RunTestSequence(
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      WaitForElementExists(webcontents_id_, kMouseRowQuery),
      ClickElement(webcontents_id_, kMouseRowQuery),
      WaitForStateChange(webcontents_id_, scrolling_speed_slider_disabled),
      ClickElement(webcontents_id_, kControlledScrollingButtonQuery),
      WaitForStateChange(webcontents_id_, scrolling_speed_slider_enabled));
}

IN_PROC_BROWSER_TEST_F(DeviceSettingsInteractiveUiTest, KeyboardFkeys) {
  const DeepQuery kF11DropdownQuery{
      "os-settings-ui",       "os-settings-main", "main-page-container",
      "settings-device-page", "#remap-keys",      "#f11",
      "#keyDropdown",         "#dropdownMenu",
  };

  const DeepQuery kF12DropdownQuery{
      "os-settings-ui",       "os-settings-main", "main-page-container",
      "settings-device-page", "#remap-keys",      "#f12",
      "#keyDropdown",         "#dropdownMenu",
  };

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDevToolsId);

  RunTestSequence(
      SetupInternalKeyboard(),
      LaunchSettingsApp(
          webcontents_id_,
          chromeos::settings::mojom::kPerDeviceKeyboardSubpagePath),
      Log("Waiting for internal keyboard to exist"),
      WaitForElementTextContains(webcontents_id_, kKeyboardNameQuery,
                                 "Built-in Keyboard"),
      Log("Navigating to 'Customize keyboard keys' subpage"),
      ClickElement(webcontents_id_, kCustomizeKeyboardKeysInternalQuery),
      Log("Remapping the 'F11' action to the shortcut that uses the shift "
          "modifier"),
      ScrollIntoView(webcontents_id_, kF11DropdownQuery),
      ExecuteJsAt(webcontents_id_, kF11DropdownQuery,
                  "(el) => {el.selectedIndex = 1; el.dispatchEvent(new "
                  "Event('change'));}"),
      Log("Verifying 'F11' action contains the shift shortcut"),
      WaitForDropdownContainsValue(kF11DropdownQuery, /*value=*/1),
      SendKeyPressAndReleaseEvent(ui::VKEY_F1,
                                  ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN),
      ScrollIntoView(webcontents_id_, kF11DropdownQuery),
      Log("Remapping the 'F12' action to the shortcut that uses the shift "
          "modifier"),
      ExecuteJsAt(webcontents_id_, kF12DropdownQuery,
                  "(el) => {el.selectedIndex = 1; el.dispatchEvent(new "
                  "Event('change'));}"),
      Log("Verifying 'F12' action contains the shift shortcut"),
      WaitForDropdownContainsValue(kF12DropdownQuery, /*value=*/1),
      SendKeyPressAndReleaseEvent(ui::VKEY_F2,
                                  ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN),
      InstrumentNextTab(kDevToolsId, AnyBrowser()),
      Log("Verifying that 'F12' shortcut opens the developer console"),
      WaitForShow(kDevToolsId));
  // Get settings browser and verify that the window is maximized.
  Browser* browser = BrowserList::GetInstance()->get(0);
  EXPECT_TRUE(browser->window()->IsFullscreen());
}

}  // namespace

}  // namespace ash
