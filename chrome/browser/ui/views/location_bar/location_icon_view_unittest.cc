// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "ui/views/widget/widget.h"

namespace {

class TestLocationIconDelegate : public IconLabelBubbleView::Delegate,
                                 public LocationIconView::Delegate {
 public:
  explicit TestLocationIconDelegate(LocationBarModel* location_bar_model)
      : location_bar_model_(location_bar_model) {}
  virtual ~TestLocationIconDelegate() = default;

  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override {
    return SK_ColorBLACK;
  }
  SkColor GetIconLabelBubbleBackgroundColor() const override {
    return SK_ColorWHITE;
  }

  // LocationIconView::Delegate:
  content::WebContents* GetWebContents() override { return nullptr; }
  bool IsEditingOrEmpty() const override { return is_editing_or_empty_; }
  SkColor GetSecurityChipColor(
      security_state::SecurityLevel security_level) const override {
    return GetIconLabelBubbleSurroundingForegroundColor();
  }
  bool ShowPageInfoDialog() override { return false; }
  const LocationBarModel* GetLocationBarModel() const override {
    return location_bar_model_;
  }
  ui::ImageModel GetLocationIcon(
      IconFetchedCallback on_icon_fetched) const override {
    return ui::ImageModel();
  }

  void set_is_editing_or_empty(bool is_editing_or_empty) {
    is_editing_or_empty_ = is_editing_or_empty;
  }

 private:
  LocationBarModel* location_bar_model_;
  bool is_editing_or_empty_ = false;
};

}  // namespace

class LocationIconViewTest : public ChromeViewsTestBase {
 protected:
  // ChromeViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    gfx::FontList font_list;

    widget_ = CreateTestWidget();

    location_bar_model_ = std::make_unique<TestLocationBarModel>();
    delegate_ =
        std::make_unique<TestLocationIconDelegate>(location_bar_model());

    auto view =
        std::make_unique<LocationIconView>(font_list, delegate(), delegate());
    view->SetBoundsRect(gfx::Rect(0, 0, 24, 24));
    view_ = widget_->SetContentsView(std::move(view));

    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestLocationBarModel* location_bar_model() {
    return location_bar_model_.get();
  }

  void SetSecurityLevel(security_state::SecurityLevel level) {
    location_bar_model()->set_security_level(level);

    base::string16 secure_display_text = base::string16();
    if (level == security_state::SecurityLevel::DANGEROUS ||
        level == security_state::SecurityLevel::WARNING)
      secure_display_text = base::ASCIIToUTF16("Insecure");

    location_bar_model()->set_secure_display_text(secure_display_text);
  }

  TestLocationIconDelegate* delegate() { return delegate_.get(); }
  LocationIconView* view() { return view_; }

 private:
  std::unique_ptr<TestLocationBarModel> location_bar_model_;
  std::unique_ptr<TestLocationIconDelegate> delegate_;
  LocationIconView* view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(LocationIconViewTest, ShouldNotAnimateWhenSuppressingAnimations) {
  // Make sure the initial status is secure.
  SetSecurityLevel(security_state::SecurityLevel::SECURE);
  view()->Update(/*suppress_animations=*/true);

  SetSecurityLevel(security_state::SecurityLevel::DANGEROUS);
  view()->Update(/*suppress_animations=*/true);
  // When we change tab, suppress animations is true.
  EXPECT_FALSE(view()->is_animating_label());
}

TEST_F(LocationIconViewTest, ShouldAnimateTextWhenWarning) {
  // Make sure the initial status is secure.
  SetSecurityLevel(security_state::SecurityLevel::SECURE);
  view()->Update(/*suppress_animations=*/true);

  SetSecurityLevel(security_state::SecurityLevel::WARNING);
  view()->Update(/*suppress_animations=*/false);
  EXPECT_TRUE(view()->is_animating_label());
}

TEST_F(LocationIconViewTest, ShouldAnimateTextWhenDangerous) {
  // Make sure the initial status is secure.
  SetSecurityLevel(security_state::SecurityLevel::SECURE);
  view()->Update(/*suppress_animations=*/true);

  SetSecurityLevel(security_state::SecurityLevel::DANGEROUS);
  view()->Update(/*suppress_animations=*/false);
  EXPECT_TRUE(view()->is_animating_label());
}

TEST_F(LocationIconViewTest, ShouldNotAnimateWarningToDangerous) {
  // Make sure the initial status is secure.
  SetSecurityLevel(security_state::SecurityLevel::WARNING);
  view()->Update(/*suppress_animations=*/true);

  SetSecurityLevel(security_state::SecurityLevel::DANGEROUS);
  view()->Update(/*suppress_animations=*/false);
  EXPECT_FALSE(view()->is_animating_label());
}
