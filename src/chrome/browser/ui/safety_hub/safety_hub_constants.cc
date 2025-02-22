// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "base/time/time.h"

namespace safety_hub {

const char kCardHeaderKey[] = "header";
const char kCardSubheaderKey[] = "subheader";
const char kCardStateKey[] = "state";
const char kSafetyHubSafeBrowsingStatusKey[] = "safeBrowsingStatus";

const char kSafetyHubMenuNotificationActiveKey[] = "isCurrentlyActive";
const char kSafetyHubMenuNotificationAllTimeCountKey[] = "allTimeCount";
const char kSafetyHubMenuNotificationImpressionCountKey[] = "impressionCount";
const char kSafetyHubMenuNotificationFirstImpressionKey[] =
    "firstImpressionTime";
const char kSafetyHubMenuNotificationLastImpressionKey[] = "lastImpressionTime";
const char kSafetyHubMenuNotificationShowAfterTimeKey[] = "onlyShowAfterTime";
const char kSafetyHubMenuNotificationResultKey[] = "result";

const char kSafetyHubTriggeringExtensionIdsKey[] = "triggeringExtensions";

const char kExpirationKey[] = "expiration";
const char kLifetimeKey[] = "lifetime";
const char kSafetyHubChooserPermissionsData[] = "chooserPermissionsData";

const base::TimeDelta kMinTimeBetweenPasswordChecks = base::Hours(1);

const char kRevokedStatusDictKeyStr[] = "revoked_status";
const char kIgnoreStr[] = "ignore";
const char kRevokeStr[] = "revoke";

}  // namespace safety_hub
