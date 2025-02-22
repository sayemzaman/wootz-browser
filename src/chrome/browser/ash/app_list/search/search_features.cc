// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_features.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chromeos/components/libsegmentation/buildflags.h"
#include "chromeos/constants/chromeos_features.h"

namespace search_features {

BASE_FEATURE(kLauncherGameSearch,
             "LauncherGameSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherKeywordExtractionScoring,
             "LauncherKeywordExtractionScoring",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherQueryFederatedAnalyticsPHH,
             "LauncherQueryFederatedAnalyticsPHH",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherFuzzyMatchAcrossProviders,
             "LauncherFuzzyMatchAcrossProviders",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherFuzzyMatchForOmnibox,
             "LauncherFuzzyMatchForOmnibox",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherImageSearch,
             "LauncherImageSearch",
#if BUILDFLAG(ENABLE_MERGE_REQUEST)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else   //  BUILDFLAG(ENABLE_MERGE_REQUEST)
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(ENABLE_MERGE_REQUEST)

BASE_FEATURE(kLauncherLocalImageSearchConfidence,
             "LauncherLocalImageSearchConfidence",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherLocalImageSearchRelevance,
             "LauncherLocalImageSearchRelevance",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherImageSearchIca,
             "LauncherImageSearchIca",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kICASupportedByHardware,
             "ICASupportedByHardware",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherImageSearchOcr,
             "LauncherImageSearchOcr",
#if BUILDFLAG(ENABLE_MERGE_REQUEST)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else   //  BUILDFLAG(ENABLE_MERGE_REQUEST)
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(ENABLE_MERGE_REQUEST)

BASE_FEATURE(kLauncherImageSearchIndexingLimit,
             "LauncherImageSearchIndexingLimit",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherSystemInfoAnswerCards,
             "LauncherSystemInfoAnswerCards",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherSearchFileScan,
             "kLauncherSearchFileScan",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(b/330386392): kLauncherGameSearch can be removed because if there's no
// payload, there will be no result.
bool IsLauncherGameSearchEnabled() {
  return base::FeatureList::IsEnabled(kLauncherGameSearch) ||
         chromeos::features::IsCloudGamingDeviceEnabled() ||
         chromeos::features::IsAlmanacLauncherPayloadEnabled();
}

bool IsLauncherKeywordExtractionScoringEnabled() {
  return base::FeatureList::IsEnabled(kLauncherKeywordExtractionScoring);
}

bool IsLauncherQueryFederatedAnalyticsPHHEnabled() {
  return base::FeatureList::IsEnabled(kLauncherQueryFederatedAnalyticsPHH);
}

bool IsLauncherFuzzyMatchAcrossProvidersEnabled() {
  return base::FeatureList::IsEnabled(kLauncherFuzzyMatchAcrossProviders);
}

bool IsLauncherFuzzyMatchForOmniboxEnabled() {
  return base::FeatureList::IsEnabled(kLauncherFuzzyMatchForOmnibox);
}

// Only enable image search for ICA supported devices.
bool IsLauncherImageSearchEnabled() {
  return base::FeatureList::IsEnabled(
             ash::features::kFeatureManagementLocalImageSearch) &&
         base::FeatureList::IsEnabled(kLauncherImageSearch);
}

bool IsLauncherImageSearchIcaEnabled() {
  return base::FeatureList::IsEnabled(kLauncherImageSearchIca) &&
         base::FeatureList::IsEnabled(kICASupportedByHardware);
}

bool IsLauncherImageSearchOcrEnabled() {
  return base::FeatureList::IsEnabled(kLauncherImageSearchOcr);
}

bool IsLauncherImageSearchIndexingLimitEnabled() {
  return base::FeatureList::IsEnabled(kLauncherImageSearchIndexingLimit);
}

bool IsLauncherSystemInfoAnswerCardsEnabled() {
  return base::FeatureList::IsEnabled(kLauncherSystemInfoAnswerCards);
}

bool IsLauncherSearchFileScanEnabled() {
  return base::FeatureList::IsEnabled(kLauncherSearchFileScan);
}

}  // namespace search_features
