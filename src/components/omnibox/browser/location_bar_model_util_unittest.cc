// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_util.h"

#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithNoneLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::NONE,
      /*use_updated_connection_security_indicators=*/false,
      /*malicious_content_status=*/
      security_state::MALICIOUS_CONTENT_STATUS_NONE);
  EXPECT_EQ(icon.name, features::IsChromeRefresh2023()
                           ? omnibox::kHttpChromeRefreshIcon.name
                           : omnibox::kHttpIcon.name);
}

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithSecureLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::SECURE,
      /*use_updated_connection_security_indicators=*/false,
      /*malicious_content_status=*/
      security_state::MALICIOUS_CONTENT_STATUS_NONE);
  EXPECT_EQ(icon.name, features::IsChromeRefresh2023()
                           ? omnibox::kSecurePageInfoChromeRefreshIcon.name
                           : vector_icons::kHttpsValidIcon.name);
}

TEST(LocationBarModelUtilTest,
     GetSecurityVectorIconWithSecureWithPolicyInstalledCertLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT,
      /*use_updated_connection_security_indicators=*/false,
      /*malicious_content_status=*/
      security_state::MALICIOUS_CONTENT_STATUS_NONE);
  EXPECT_EQ(icon.name, features::IsChromeRefresh2023()
                           ? vector_icons::kBusinessChromeRefreshIcon.name
                           : vector_icons::kBusinessIcon.name);
}

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithDangerousLevel) {
  base::test::ScopedFeatureList scoped_feature_list_;
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::DANGEROUS,
      /*use_updated_connection_security_indicators=*/false,
      /*malicious_content_status=*/
      security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING);
  EXPECT_EQ(icon.name, features::IsChromeRefresh2023()
                           ? vector_icons::kDangerousChromeRefreshIcon.name
                           : vector_icons::kDangerousIcon.name);
}

TEST(LocationBarModelUtilTest,
     GetSecurityVectorIconBillingInterstitialWithDangerousLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::DANGEROUS,
      /*use_updated_connection_security_indicators=*/false,
      /*malicious_content_status=*/
      security_state::MALICIOUS_CONTENT_STATUS_BILLING);
  EXPECT_EQ(icon.name,
            features::IsChromeRefresh2023()
                ? vector_icons::kNotSecureWarningChromeRefreshIcon.name
                : vector_icons::kNotSecureWarningIcon.name);
}

TEST(LocationBarModelUtilTest, GetSecurityVectorIconWithWarningLevel) {
  const gfx::VectorIcon& icon = location_bar_model::GetSecurityVectorIcon(
      security_state::SecurityLevel::WARNING,
      /*use_updated_connection_security_indicators=*/false,
      /*malicious_content_status=*/
      security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING);
  EXPECT_EQ(icon.name,
            features::IsChromeRefresh2023()
                ? vector_icons::kNotSecureWarningChromeRefreshIcon.name
                : vector_icons::kNotSecureWarningIcon.name);
}
