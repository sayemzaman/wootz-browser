// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_FAKE_SAFE_BROWSING_CLIENT_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_FAKE_SAFE_BROWSING_CLIENT_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#include "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"

class SafeBrowsingService;

// Fake implementation of SafeBrowsingClient.
class FakeSafeBrowsingClient : public SafeBrowsingClient {
 public:
  FakeSafeBrowsingClient();
  ~FakeSafeBrowsingClient() override;

  // SafeBrowsingClient implementation.
  base::WeakPtr<SafeBrowsingClient> AsWeakPtr() override;

  // Controls the return value of `ShouldBlockUnsafeResource`.
  void set_should_block_unsafe_resource(bool should_block_unsafe_resource) {
    should_block_unsafe_resource_ = should_block_unsafe_resource;
  }

  // Controls the return value of `GetRealTimeUrlLookupService`.
  void set_real_time_url_lookup_service(
      safe_browsing::RealTimeUrlLookupService* lookup_service) {
    lookup_service_ = lookup_service;
  }

  // Whether `OnMainFrameUrlQueryCancellationDecided` was called.
  bool main_frame_cancellation_decided_called() {
    return main_frame_cancellation_decided_called_;
  }

 private:
  // SafeBrowsingClient implementation.
  SafeBrowsingService* GetSafeBrowsingService() override;
  safe_browsing::RealTimeUrlLookupService* GetRealTimeUrlLookupService()
      override;
  safe_browsing::HashRealTimeService* GetHashRealTimeService() override;
  variations::VariationsService* GetVariationsService() override;
  bool ShouldBlockUnsafeResource(
      const security_interstitials::UnsafeResource& resource) const override;
  void OnMainFrameUrlQueryCancellationDecided(web::WebState* web_state,
                                              const GURL& url) override;

  scoped_refptr<SafeBrowsingService> safe_browsing_service_;
  bool should_block_unsafe_resource_ = false;
  raw_ptr<safe_browsing::RealTimeUrlLookupService> lookup_service_ = nullptr;
  bool main_frame_cancellation_decided_called_ = false;

  // Must be last.
  base::WeakPtrFactory<FakeSafeBrowsingClient> weak_factory_{this};
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_FAKE_SAFE_BROWSING_CLIENT_H_
