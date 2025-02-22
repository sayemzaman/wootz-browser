// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_field_promo_controller_impl.h"

#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_view.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

AutofillFieldPromoControllerImpl::AutofillFieldPromoControllerImpl(
    content::WebContents* web_contents,
    const base::Feature& feature_promo,
    ui::ElementIdentifier promo_element_identifier)
    : web_contents_(web_contents),
      feature_promo_(feature_promo),
      promo_element_identifier_(std::move(promo_element_identifier)) {}

AutofillFieldPromoControllerImpl::~AutofillFieldPromoControllerImpl() {
  Hide();
}

void AutofillFieldPromoControllerImpl::Show(const gfx::RectF& bounds) {
  Hide();

  content::RenderFrameHost* rfh = web_contents_->GetFocusedFrame();
  if (!rfh) {
    return;
  }

  AutofillPopupHideHelper::HidingParams hiding_params = {
      .hide_on_text_field_change = false,
      // TODO(b/313587343): Maybe make this true when clicking on the IPH
      // doesn't trigger anymore the event of web contents losing focus.
      .hide_on_web_contents_lost_focus = false};
  AutofillPopupHideHelper::HidingCallback hiding_callback =
      base::BindRepeating([](AutofillFieldPromoControllerImpl& controller,
                             SuggestionHidingReason) { controller.Hide(); },
                          std::ref(*this));
  AutofillPopupHideHelper::PictureInPictureDetectionCallback
      pip_detection_callback = base::BindRepeating(
          [](AutofillFieldPromoControllerImpl& controller) {
            return controller.promo_view_ &&
                   controller.promo_view_->OverlapsWithPictureInPictureWindow();
          },
          std::ref(*this));
  promo_hide_helper_.emplace(
      web_contents_, rfh->GetGlobalId(), std::move(hiding_params),
      std::move(hiding_callback), std::move(pip_detection_callback));
  promo_view_ = AutofillFieldPromoView::CreateAndShow(
      web_contents_, bounds, promo_element_identifier_);

  if (!chrome::FindBrowserWithTab(web_contents_)
           ->window()
           ->MaybeShowFeaturePromo(feature_promo_.get())) {
    // Destroy the invisible view if the promo was not shown.
    Hide();
  }
}

void AutofillFieldPromoControllerImpl::Hide() {
  promo_hide_helper_.reset();
  if (promo_view_) {
    promo_view_->Close();
    promo_view_ = nullptr;
  }
}

}  // namespace autofill
