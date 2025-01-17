// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_factory_impl.h"

#include "base/feature_list.h"
#include "base/guid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/infobars/core/infobar.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/translate/core/browser/mock_translate_infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"
#include "ios/chrome/browser/infobars/test/mock_infobar_delegate.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_card_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/translate_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/update_password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/translate_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/test/fake_infobar_ui_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarDelegate;
using confirm_infobar_overlays::ConfirmBannerRequestConfig;
using save_card_infobar_overlays::SaveCardBannerRequestConfig;
using translate_infobar_overlays::TranslateBannerRequestConfig;
using save_card_infobar_overlays::SaveCardModalRequestConfig;
using translate_infobar_overlays::TranslateModalRequestConfig;

// Test fixture for InfobarOverlayRequestFactoryImpl.
class InfobarOverlayRequestFactoryImplTest : public PlatformTest {
 public:
  InfobarOverlayRequestFactoryImplTest()
      : prefs_(autofill::test::PrefServiceForTesting()),
        card_(base::GenerateGUID(), "https://www.example.com/"),
        translate_delegate_factory_("fr", "en") {}

  InfobarOverlayRequestFactory* factory() { return &factory_; }

 protected:
  InfobarOverlayRequestFactoryImpl factory_;
  std::unique_ptr<PrefService> prefs_;
  autofill::CreditCard card_;
  std::unique_ptr<InfoBarIOS> infobar_;
  translate::testing::MockTranslateInfoBarDelegateFactory
      translate_delegate_factory_;
};

// Tests that the factory creates a save passwords infobar request.
TEST_F(InfobarOverlayRequestFactoryImplTest, SavePasswords) {
  GURL url("https://chromium.test");
  std::unique_ptr<InfoBarDelegate> delegate =
      MockIOSChromeSavePasswordInfoBarDelegate::Create(@"username", @"password",
                                                       url);
  InfoBarIOS infobar(InfobarType::kInfobarTypePasswordSave,
                     std::move(delegate));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kBanner);
  EXPECT_TRUE(banner_request
                  ->GetConfig<SavePasswordInfobarBannerOverlayRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kModal);
  EXPECT_TRUE(
      modal_request->GetConfig<PasswordInfobarModalOverlayRequestConfig>());
}

// Tests that the factory creates an update passwords infobar request.
TEST_F(InfobarOverlayRequestFactoryImplTest, UpdatePasswords) {
  GURL url("https://chromium.test");
  std::unique_ptr<InfoBarDelegate> delegate =
      MockIOSChromeSavePasswordInfoBarDelegate::Create(@"username", @"password",
                                                       url);
  InfoBarIOS infobar(InfobarType::kInfobarTypePasswordUpdate,
                     std::move(delegate));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kBanner);
  EXPECT_TRUE(
      banner_request
          ->GetConfig<UpdatePasswordInfobarBannerOverlayRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kModal);
  EXPECT_TRUE(
      modal_request->GetConfig<PasswordInfobarModalOverlayRequestConfig>());
}

// Tests that the factory creates an confirm infobar request.
TEST_F(InfobarOverlayRequestFactoryImplTest, Confirm) {
  GURL url("https://chromium.test");
  std::unique_ptr<MockInfobarDelegate> delegate =
      std::make_unique<MockInfobarDelegate>();
  InfoBarIOS infobar(InfobarType::kInfobarTypeConfirm, std::move(delegate));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kBanner);
  EXPECT_TRUE(banner_request->GetConfig<ConfirmBannerRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kModal);
  EXPECT_FALSE(modal_request);
}

// Tests that the factory creates a save card request.
TEST_F(InfobarOverlayRequestFactoryImplTest, SaveCard) {
  GURL url("https://chromium.test");
  InfoBarIOS infobar(InfobarType::kInfobarTypeSaveCard,
                     MockAutofillSaveCardInfoBarDelegateMobileFactory::
                         CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(
                             false, prefs_.get(), card_));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kBanner);
  EXPECT_TRUE(banner_request->GetConfig<SaveCardBannerRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kModal);
  EXPECT_TRUE(modal_request->GetConfig<SaveCardModalRequestConfig>());
}

// Tests that the factory creates a translate request.
TEST_F(InfobarOverlayRequestFactoryImplTest, Translate) {
  GURL url("https://chromium.test");
  InfoBarIOS infobar(
      InfobarType::kInfobarTypeTranslate,
      translate_delegate_factory_.CreateMockTranslateInfoBarDelegate(
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kBanner);
  EXPECT_TRUE(banner_request->GetConfig<TranslateBannerRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kModal);
  EXPECT_TRUE(modal_request->GetConfig<TranslateModalRequestConfig>());
}
