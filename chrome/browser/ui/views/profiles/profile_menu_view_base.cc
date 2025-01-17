// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/profiles/incognito_menu_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/native_theme/themed_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/views/sync/dice_signin_button_view.h"
#endif

namespace {

ProfileMenuViewBase* g_profile_bubble_ = nullptr;

// Helpers --------------------------------------------------------------------

constexpr int kMenuWidth = 288;
constexpr int kMaxImageSize = ProfileMenuViewBase::kIdentityImageSize;
constexpr int kDefaultMargin = 8;
constexpr int kBadgeSize = 16;
constexpr int kCircularImageButtonSize = 28;
// TODO(crbug.com/1128499): Remove this constant by extracting art height from
// |avatar_header_art|.
constexpr int kHeaderArtHeight = 80;
constexpr int kIdentityImageBorder = 2;
constexpr int kIdentityImageSizeInclBorder =
    ProfileMenuViewBase::kIdentityImageSize + 2 * kIdentityImageBorder;
constexpr int kHalfOfAvatarImageViewSize = kIdentityImageSizeInclBorder / 2;

// If the bubble is too large to fit on the screen, it still needs to be at
// least this tall to show one row.
constexpr int kMinimumScrollableContentHeight = 40;

// Spacing between the edge of the user menu and the top/bottom or left/right of
// the menu items.
constexpr int kMenuEdgeMargin = 16;

gfx::ImageSkia SizeImage(const gfx::ImageSkia& image, int size) {
  return gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, gfx::Size(size, size));
}

gfx::ImageSkia ColorImage(const gfx::ImageSkia& image, SkColor color) {
  return gfx::ImageSkiaOperations::CreateColorMask(image, color);
}

class CircleImageSource : public gfx::CanvasImageSource {
 public:
  CircleImageSource(int size, SkColor color)
      : gfx::CanvasImageSource(gfx::Size(size, size)), color_(color) {}
  ~CircleImageSource() override = default;

  void Draw(gfx::Canvas* canvas) override;

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(CircleImageSource);
};

void CircleImageSource::Draw(gfx::Canvas* canvas) {
  float radius = size().width() / 2.0f;
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(color_);
  canvas->DrawCircle(gfx::PointF(radius, radius), radius, flags);
}

gfx::ImageSkia CreateCircle(int size, SkColor color = SK_ColorWHITE) {
  return gfx::CanvasImageSource::MakeImageSkia<CircleImageSource>(size, color);
}

gfx::ImageSkia CropCircle(const gfx::ImageSkia& image) {
  DCHECK_EQ(image.width(), image.height());
  return gfx::ImageSkiaOperations::CreateMaskedImage(
      image, CreateCircle(image.width()));
}

gfx::ImageSkia AddCircularBackground(const gfx::ImageSkia& image,
                                     SkColor bg_color,
                                     int size) {
  if (image.isNull())
    return gfx::ImageSkia();

  return gfx::ImageSkiaOperations::CreateSuperimposedImage(
      CreateCircle(size, bg_color), image);
}

std::unique_ptr<views::BoxLayout> CreateBoxLayout(
    views::BoxLayout::Orientation orientation,
    views::BoxLayout::CrossAxisAlignment cross_axis_alignment,
    gfx::Insets insets = gfx::Insets()) {
  auto layout = std::make_unique<views::BoxLayout>(orientation, insets);
  layout->set_cross_axis_alignment(cross_axis_alignment);
  return layout;
}

const gfx::ImageSkia ImageForMenu(const gfx::VectorIcon& icon,
                                  float icon_to_image_ratio,
                                  SkColor color) {
  const int padding =
      static_cast<int>(kMaxImageSize * (1.0f - icon_to_image_ratio) / 2.0f);

  gfx::ImageSkia sized_icon =
      gfx::CreateVectorIcon(icon, kMaxImageSize - 2 * padding, color);
  return gfx::CanvasImageSource::CreatePadded(sized_icon, gfx::Insets(padding));
}

gfx::ImageSkia SizeImageModel(const ui::ImageModel& image_model,
                              const ui::NativeTheme* native_theme,
                              int size) {
  return image_model.IsImage()
             ? CropCircle(SizeImage(image_model.GetImage().AsImageSkia(), size))
             : ui::ThemedVectorIcon(image_model.GetVectorIcon())
                   .GetImageSkia(native_theme, size);
}

// TODO(crbug.com/1146998): Adjust button size to be 16x16.
class CircularImageButton : public views::ImageButton {
 public:
  METADATA_HEADER(CircularImageButton);

  CircularImageButton(PressedCallback callback,
                      const gfx::VectorIcon& icon,
                      const base::string16& text,
                      SkColor background_profile_color = SK_ColorTRANSPARENT,
                      bool show_border = false)
      : ImageButton(std::move(callback)),
        icon_(icon),
        background_profile_color_(background_profile_color),
        show_border_(show_border) {
    SetTooltipText(text);
    SetInkDropMode(views::Button::InkDropMode::ON);

    InstallCircleHighlightPathGenerator(this);
  }

  // views::ImageButton:
  void OnThemeChanged() override {
    views::ImageButton::OnThemeChanged();
    constexpr float kShortcutIconToImageRatio = 9.0f / 16.0f;
    const int kBorderThickness = show_border_ ? 1 : 0;
    const SkScalar kButtonRadius =
        (kCircularImageButtonSize + 2 * kBorderThickness) / 2.0f;

    SkColor icon_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_DefaultIconColor);
    if (background_profile_color_ != SK_ColorTRANSPARENT)
      icon_color = GetProfileForegroundIconColor(background_profile_color_);
    gfx::ImageSkia image =
        ImageForMenu(icon_, kShortcutIconToImageRatio, icon_color);
    SetImage(views::Button::STATE_NORMAL,
             SizeImage(image, kCircularImageButtonSize));
    SetInkDropBaseColor(icon_color);

    if (show_border_) {
      const SkColor separator_color = GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_MenuSeparatorColor);
      SetBorder(views::CreateRoundedRectBorder(kBorderThickness, kButtonRadius,
                                               separator_color));
    }
  }

 private:
  const gfx::VectorIcon& icon_;
  const SkColor background_profile_color_;
  bool show_border_;
};

BEGIN_METADATA(CircularImageButton, views::ImageButton)
END_METADATA

class FeatureButtonIconView : public views::ImageView {
 public:
  FeatureButtonIconView(const gfx::VectorIcon& icon, float icon_to_image_ratio)
      : icon_(icon), icon_to_image_ratio_(icon_to_image_ratio) {}
  ~FeatureButtonIconView() override = default;

  // views::ImageView:
  void OnThemeChanged() override {
    views::ImageView::OnThemeChanged();
    constexpr int kIconSize = 16;
    const SkColor icon_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_DefaultIconColor);
    gfx::ImageSkia image =
        ImageForMenu(icon_, icon_to_image_ratio_, icon_color);
    SetImage(SizeImage(ColorImage(image, icon_color), kIconSize));
  }

 private:
  const gfx::VectorIcon& icon_;
  const float icon_to_image_ratio_;
};

class ProfileManagementIconView : public views::ImageView {
 public:
  explicit ProfileManagementIconView(const gfx::VectorIcon& icon)
      : icon_(icon) {}
  ~ProfileManagementIconView() override = default;

  // views::ImageView:
  void OnThemeChanged() override {
    views::ImageView::OnThemeChanged();
    constexpr float kIconToImageRatio = 0.75f;
    constexpr int kIconSize = 20;
    const SkColor icon_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_DefaultIconColor);
    gfx::ImageSkia image = ImageForMenu(icon_, kIconToImageRatio, icon_color);
    SetImage(SizeImage(image, kIconSize));
  }

 private:
  const gfx::VectorIcon& icon_;
};

// AvatarImageView is used to ensure avatar adornments are kept in sync with
// current theme colors.
class AvatarImageView : public views::ImageView {
 public:
  AvatarImageView(const ui::ImageModel& avatar_image,
                  const ProfileMenuViewBase* root_view)
      : avatar_image_(avatar_image), root_view_(root_view) {
    if (avatar_image_.IsEmpty()) {
      // This can happen if the account image hasn't been fetched yet, if there
      // is no image, or in tests.
      avatar_image_ = ui::ImageModel::FromVectorIcon(
          kUserAccountAvatarIcon, ui::NativeTheme::kColorId_MenuIconColor,
          ProfileMenuViewBase::kIdentityImageSize);
    }
  }

  // views::ImageView:
  void OnThemeChanged() override {
    ImageView::OnThemeChanged();
    constexpr int kBadgePadding = 1;
    DCHECK(!avatar_image_.IsEmpty());
    gfx::ImageSkia sized_avatar_image =
        SizeImageModel(avatar_image_, GetNativeTheme(),
                       ProfileMenuViewBase::kIdentityImageSize);
    if (base::FeatureList::IsEnabled(features::kNewProfilePicker)) {
      sized_avatar_image =
          AddCircularBackground(sized_avatar_image, GetBackgroundColor(),
                                kIdentityImageSizeInclBorder);
    }
    gfx::ImageSkia sized_badge = AddCircularBackground(
        SizeImage(root_view_->GetSyncIcon(), kBadgeSize), GetBackgroundColor(),
        kBadgeSize + 2 * kBadgePadding);
    gfx::ImageSkia sized_badge_with_shadow =
        gfx::ImageSkiaOperations::CreateImageWithDropShadow(
            sized_badge, gfx::ShadowValue::MakeMdShadowValues(/*elevation=*/1,
                                                              SK_ColorBLACK));

    gfx::ImageSkia badged_image = gfx::ImageSkiaOperations::CreateIconWithBadge(
        sized_avatar_image, sized_badge_with_shadow);
    SetImage(badged_image);
  }

 private:
  SkColor GetBackgroundColor() const {
    return GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_BubbleBackground);
  }

  ui::ImageModel avatar_image_;
  const ProfileMenuViewBase* root_view_;
};

class SyncButton : public HoverButton {
 public:
  METADATA_HEADER(SyncButton);
  SyncButton(PressedCallback callback,
             ProfileMenuViewBase* root_view,
             const base::string16& clickable_text)
      : HoverButton(std::move(callback), clickable_text),
        root_view_(root_view) {}

  // HoverButton:
  void OnThemeChanged() override {
    HoverButton::OnThemeChanged();
    SetImage(STATE_NORMAL, SizeImage(root_view_->GetSyncIcon(), kBadgeSize));
  }

 private:
  const ProfileMenuViewBase* root_view_;
};

BEGIN_METADATA(SyncButton, HoverButton)
END_METADATA

class SyncImageView : public views::ImageView {
 public:
  explicit SyncImageView(const ProfileMenuViewBase* root_view)
      : root_view_(root_view) {}

  // views::ImageView:
  void OnThemeChanged() override {
    ImageView::OnThemeChanged();
    SetImage(SizeImage(root_view_->GetSyncIcon(), kBadgeSize));
  }

 private:
  const ProfileMenuViewBase* root_view_;
};

void BuildProfileTitleAndSubtitle(views::View* parent,
                                  const base::string16& title,
                                  const base::string16& subtitle) {
  views::View* profile_titles_container =
      parent->AddChildView(std::make_unique<views::View>());
  // Separate the titles from the avatar image by the default margin.
  profile_titles_container->SetLayoutManager(
      CreateBoxLayout(views::BoxLayout::Orientation::kVertical,
                      views::BoxLayout::CrossAxisAlignment::kCenter,
                      gfx::Insets(kDefaultMargin, 0, 0, 0)));

  if (!title.empty()) {
    profile_titles_container->AddChildView(std::make_unique<views::Label>(
        title, views::style::CONTEXT_DIALOG_TITLE));
  }

  if (!subtitle.empty()) {
    profile_titles_container->AddChildView(std::make_unique<views::Label>(
        subtitle, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  }
}

// This function deals with the somewhat complicted layout to build the part of
// the profile identity info that has a colored background.
void BuildProfileBackgroundContainer(
    views::View* parent,
    std::unique_ptr<views::View> heading_label,
    base::Optional<SkColor> background_color,
    std::unique_ptr<views::View> avatar_image_view,
    std::unique_ptr<views::View> edit_button,
    const ui::ThemedVectorIcon& avatar_header_art) {

  views::View* profile_background_container =
      parent->AddChildView(std::make_unique<views::View>());

  gfx::Insets background_container_insets(0, /*horizontal=*/kMenuEdgeMargin);
  if (edit_button) {
    // Compensate for the edit button on the right with an extra margin on the
    // left so that the rest is centered.
    background_container_insets.set_left(background_container_insets.left() +
                                         kCircularImageButtonSize);
  }
  profile_background_container
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kEnd)
      .SetInteriorMargin(background_container_insets);
  if (background_color.has_value()) {
    // The bottom background edge should match the center of the identity image.
    gfx::Insets background_insets(0, 0, /*bottom=*/kHalfOfAvatarImageViewSize,
                                  0);
    // TODO(crbug.com/1147038): Remove the zero-radius rounded background.
    profile_background_container->SetBackground(
        views::CreateBackgroundFromPainter(
            views::Painter::CreateSolidRoundRectPainter(
                background_color.value(), /*radius=*/0, background_insets)));
  } else {
    profile_background_container->SetBackground(
        views::CreateThemedVectorIconBackground(profile_background_container,
                                                avatar_header_art));
  }

  // |avatar_margin| is derived from |avatar_header_art| asset height, it
  // increases margin for the avatar icon to make |avatar_header_art| visible
  // above the center of the avatar icon.
  const int avatar_margin = avatar_header_art.empty()
                                ? kMenuEdgeMargin
                                : kHeaderArtHeight - kHalfOfAvatarImageViewSize;

  // The |heading_and_image_container| is on the left and it stretches almost
  // the full width. It contains the profile heading and the avatar image.
  views::View* heading_and_image_container =
      profile_background_container->AddChildView(
          std::make_unique<views::View>());
  heading_and_image_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(1));
  heading_and_image_container
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(gfx::Insets(/*top=*/avatar_margin, 0, 0, 0));
  if (heading_label) {
    DCHECK(avatar_header_art.empty());
    heading_label->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(/*vertical=*/kDefaultMargin, 0)));
    heading_and_image_container->AddChildView(std::move(heading_label));
  }

  heading_and_image_container->AddChildView(std::move(avatar_image_view));

  // The |edit_button| is on the right and has fixed width.
  if (edit_button) {
    edit_button->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kPreferred)
            .WithOrder(2));
    views::View* edit_button_container =
        profile_background_container->AddChildView(
            std::make_unique<views::View>());
    edit_button_container->SetLayoutManager(CreateBoxLayout(
        views::BoxLayout::Orientation::kVertical,
        views::BoxLayout::CrossAxisAlignment::kCenter,
        gfx::Insets(
            0, 0, /*bottom=*/kHalfOfAvatarImageViewSize + kDefaultMargin, 0)));
    edit_button_container->AddChildView(std::move(edit_button));
  }
}

}  // namespace

// ProfileMenuViewBase ---------------------------------------------------------

ProfileMenuViewBase::EditButtonParams::EditButtonParams(
    const gfx::VectorIcon* edit_icon,
    const base::string16& edit_tooltip_text,
    base::RepeatingClosure edit_action)
    : edit_icon(edit_icon),
      edit_tooltip_text(edit_tooltip_text),
      edit_action(edit_action) {}

ProfileMenuViewBase::EditButtonParams::~EditButtonParams() = default;

ProfileMenuViewBase::EditButtonParams::EditButtonParams(
    const EditButtonParams&) = default;

// static
void ProfileMenuViewBase::ShowBubble(
    profiles::BubbleViewMode view_mode,
    views::Button* anchor_button,
    Browser* browser,
    bool is_source_keyboard) {
  if (IsShowing())
    return;

  signin_ui_util::RecordProfileMenuViewShown(browser->profile());

  ProfileMenuViewBase* bubble;

  if (view_mode == profiles::BUBBLE_VIEW_MODE_INCOGNITO) {
    DCHECK(browser->profile()->IsIncognitoProfile());
    bubble = new IncognitoMenuView(anchor_button, browser);
  } else {
    DCHECK_EQ(profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER, view_mode);
    bubble = new ProfileMenuView(anchor_button, browser);
  }

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->ax_widget_observer_ =
      std::make_unique<AXMenuWidgetObserver>(bubble, widget);
  widget->Show();
  if (is_source_keyboard)
    bubble->FocusButtonOnKeyboardOpen();
}

// static
bool ProfileMenuViewBase::IsShowing() {
  return g_profile_bubble_ != nullptr;
}

// static
void ProfileMenuViewBase::Hide() {
  if (g_profile_bubble_)
    g_profile_bubble_->GetWidget()->Close();
}

// static
ProfileMenuViewBase* ProfileMenuViewBase::GetBubbleForTesting() {
  return g_profile_bubble_;
}

ProfileMenuViewBase::ProfileMenuViewBase(views::Button* anchor_button,
                                         Browser* browser)
    : BubbleDialogDelegateView(anchor_button, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      anchor_button_(anchor_button),
      close_bubble_helper_(this, browser) {
  DCHECK(!g_profile_bubble_);
  g_profile_bubble_ = this;
  SetButtons(ui::DIALOG_BUTTON_NONE);
  // TODO(tluk): Remove when fixing https://crbug.com/822075
  // The sign in webview will be clipped on the bottom corners without these
  // margins, see related bug <http://crbug.com/593203>.
  SetPaintClientToLayer(true);
  set_margins(gfx::Insets(0));
  DCHECK(anchor_button);
  anchor_button->AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);

  SetEnableArrowKeyTraversal(true);
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kMenu);

  RegisterWindowClosingCallback(base::BindOnce(
      &ProfileMenuViewBase::OnWindowClosing, base::Unretained(this)));
}

ProfileMenuViewBase::~ProfileMenuViewBase() {
  // Items stored for menu generation are removed after menu is finalized, hence
  // it's not expected to have while destroying the object.
  DCHECK(g_profile_bubble_ != this);
}

gfx::ImageSkia ProfileMenuViewBase::GetSyncIcon() const {
  return gfx::ImageSkia();
}

void ProfileMenuViewBase::SetProfileIdentityInfo(
    const base::string16& profile_name,
    SkColor profile_background_color,
    base::Optional<EditButtonParams> edit_button_params,
    const ui::ImageModel& image_model,
    const base::string16& title,
    const base::string16& subtitle,
    const ui::ThemedVectorIcon& avatar_header_art) {
  constexpr int kBottomMargin = kDefaultMargin;
  const bool new_design =
      base::FeatureList::IsEnabled(features::kNewProfilePicker);

  identity_info_container_->RemoveAllChildViews(/*delete_children=*/true);
  // In the new design, the colored background fully bleeds to the edges of the
  // menu and to achieve that |container_margin| is set to 0. In this case,
  // further margins will be added by children views.
  const int container_margin = new_design ? 0 : kMenuEdgeMargin;
  identity_info_container_->SetLayoutManager(
      CreateBoxLayout(views::BoxLayout::Orientation::kVertical,
                      views::BoxLayout::CrossAxisAlignment::kStretch,
                      gfx::Insets(container_margin, container_margin,
                                  kBottomMargin, container_margin)));

  auto avatar_image_view = std::make_unique<AvatarImageView>(image_model, this);

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // crbug.com/1161166: Orca does not read the accessible window title of the
  // bubble, so we duplicate it in the top-level menu item. To be revisited
  // after considering other options, including fixes on the AT side.
  GetViewAccessibility().OverrideName(GetAccessibleWindowTitle());
#endif

  if (!new_design) {
    if (!profile_name.empty()) {
      DCHECK(edit_button_params.has_value());
      const SkColor kBackgroundColor = GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_HighlightedMenuItemBackgroundColor);

      heading_container_->RemoveAllChildViews(/*delete_children=*/true);
      heading_container_->SetLayoutManager(
          std::make_unique<views::FillLayout>());
      heading_container_->SetBackground(
          views::CreateSolidBackground(kBackgroundColor));

      views::LabelButton* heading_button =
          heading_container_->AddChildView(std::make_unique<HoverButton>(
              base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                                  base::Unretained(this),
                                  std::move(edit_button_params->edit_action)),
              profile_name));
      heading_button->SetEnabledTextColors(views::style::GetColor(
          *this, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
      heading_button->SetTooltipText(edit_button_params->edit_tooltip_text);
      heading_button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
      heading_button->SetBorder(
          views::CreateEmptyBorder(gfx::Insets(kDefaultMargin)));
    }

    identity_info_container_->AddChildView(std::move(avatar_image_view));
    BuildProfileTitleAndSubtitle(/*parent=*/identity_info_container_, title,
                                 subtitle);
    return;
  }

  base::Optional<SkColor> background_color;
  // Only show a colored background when there is an edit button (this
  // coincides with the profile being a real profile that can be edited).
  if (edit_button_params.has_value()) {
    background_color = profile_background_color;
  }

  std::unique_ptr<views::Label> heading_label;
  if (!profile_name.empty()) {
    views::Label::CustomFont font = {
        views::Label::GetDefaultFontList()
            .DeriveWithSizeDelta(2)
            .DeriveWithWeight(gfx::Font::Weight::BOLD)};
    heading_label = std::make_unique<views::Label>(profile_name, font);
    heading_label->SetElideBehavior(gfx::ELIDE_TAIL);
    heading_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    heading_label->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    if (background_color) {
      heading_label->SetAutoColorReadabilityEnabled(false);
      heading_label->SetEnabledColor(
          GetProfileForegroundTextColor(*background_color));
    }
  }

  std::unique_ptr<views::View> edit_button;
  if (edit_button_params.has_value()) {
    edit_button = std::make_unique<CircularImageButton>(
        base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                            base::Unretained(this),
                            std::move(edit_button_params->edit_action)),
        *edit_button_params->edit_icon, edit_button_params->edit_tooltip_text,
        background_color.value_or(SK_ColorTRANSPARENT));
  }

  BuildProfileBackgroundContainer(
      /*parent=*/identity_info_container_, std::move(heading_label),
      background_color,
      std::move(avatar_image_view), std::move(edit_button), avatar_header_art);
  BuildProfileTitleAndSubtitle(/*parent=*/identity_info_container_, title,
                               subtitle);
}

void ProfileMenuViewBase::SetSyncInfo(const SyncInfo& sync_info,
                                      const base::RepeatingClosure& action,
                                      bool show_badge) {
  const base::string16 description =
      l10n_util::GetStringUTF16(sync_info.description_string_id);
  const base::string16 clickable_text =
      l10n_util::GetStringUTF16(sync_info.button_string_id);
  const int kDescriptionIconSpacing =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  constexpr int kInsidePadding = 12;
  constexpr int kBorderThickness = 1;
  const int kBorderCornerRadius =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(views::EMPHASIS_HIGH);

  sync_background_state_ = sync_info.background_state;
  UpdateSyncInfoContainerBackground();

  sync_info_container_->RemoveAllChildViews(/*delete_children=*/true);
  sync_info_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), kInsidePadding));

  if (description.empty()) {
    sync_info_container_->AddChildView(std::make_unique<SyncButton>(
        base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                            base::Unretained(this), std::move(action)),
        this, clickable_text));
    return;
  }

  const SkColor border_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_MenuSeparatorColor);
  // Add padding, rounded border and margins.
  sync_info_container_->SetBorder(views::CreatePaddedBorder(
      views::CreateRoundedRectBorder(kBorderThickness, kBorderCornerRadius,
                                     border_color),
      gfx::Insets(kInsidePadding)));
  sync_info_container_->SetProperty(
      views::kMarginsKey, gfx::Insets(kDefaultMargin, kMenuEdgeMargin));

  // Add icon + description at the top.
  views::View* description_container =
      sync_info_container_->AddChildView(std::make_unique<views::View>());
  views::BoxLayout* description_layout =
      description_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
              kDescriptionIconSpacing));

  if (show_badge) {
    description_container->AddChildView(std::make_unique<SyncImageView>(this));
  } else {
    // If there is no image, the description is centered.
    description_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
  }

  views::Label* label = description_container->AddChildView(
      std::make_unique<views::Label>(description));
  label->SetMultiLine(true);
  label->SetHandlesTooltips(false);

  // Set sync info description as the name of the parent container, so
  // accessibility tools can read it together with the button text. The role
  // change is required by Windows ATs.
  sync_info_container_->GetViewAccessibility().OverrideName(description);
  sync_info_container_->GetViewAccessibility().OverrideRole(
      ax::mojom::Role::kGroup);

  // Add the prominent button at the bottom.
  auto* button =
      sync_info_container_->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                              base::Unretained(this), std::move(action)),
          clickable_text));
  button->SetProminent(true);
}

void ProfileMenuViewBase::AddShortcutFeatureButton(
    const gfx::VectorIcon& icon,
    const base::string16& text,
    base::RepeatingClosure action) {
  const int kButtonSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  // Initialize layout if this is the first time a button is added.
  if (!shortcut_features_container_->GetLayoutManager()) {
    views::BoxLayout* layout = shortcut_features_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets(/*top=*/kDefaultMargin / 2, 0,
                        /*bottom=*/kMenuEdgeMargin, 0),
            kButtonSpacing));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
  }

  views::Button* button = shortcut_features_container_->AddChildView(
      std::make_unique<CircularImageButton>(
          base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                              base::Unretained(this), std::move(action)),
          icon, text, SK_ColorTRANSPARENT,
          /*show_border=*/true));
  button->SetFlipCanvasOnPaintForRTLUI(false);
}

void ProfileMenuViewBase::AddFeatureButton(const base::string16& text,
                                           base::RepeatingClosure action,
                                           const gfx::VectorIcon& icon,
                                           float icon_to_image_ratio) {
  // Initialize layout if this is the first time a button is added.
  if (!features_container_->GetLayoutManager()) {
    features_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
  }

  views::View* button;
  if (&icon == &gfx::kNoneIcon) {
    button = features_container_->AddChildView(std::make_unique<HoverButton>(
        base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                            base::Unretained(this), std::move(action)),
        text));
  } else {
    auto icon_view =
        std::make_unique<FeatureButtonIconView>(icon, icon_to_image_ratio);
    button = features_container_->AddChildView(std::make_unique<HoverButton>(
        base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                            base::Unretained(this), std::move(action)),
        std::move(icon_view), text));
  }
}

void ProfileMenuViewBase::SetProfileManagementHeading(
    const base::string16& heading) {
  profile_mgmt_heading_ = heading;

  // Add separator before heading.
  profile_mgmt_separator_container_->RemoveAllChildViews(
      /*delete_children=*/true);
  profile_mgmt_separator_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  profile_mgmt_separator_container_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kDefaultMargin, /*horizontal=*/0)));
  profile_mgmt_separator_container_->AddChildView(
      std::make_unique<views::Separator>());

  // Initialize heading layout.
  profile_mgmt_heading_container_->RemoveAllChildViews(
      /*delete_children=*/true);
  profile_mgmt_heading_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  profile_mgmt_heading_container_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kDefaultMargin, kMenuEdgeMargin)));

  // Add heading.
  views::Label* label = profile_mgmt_heading_container_->AddChildView(
      std::make_unique<views::Label>(heading, views::style::CONTEXT_LABEL,
                                     views::style::STYLE_HINT));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetHandlesTooltips(false);
}

void ProfileMenuViewBase::AddSelectableProfile(
    const ui::ImageModel& image_model,
    const base::string16& name,
    bool is_guest,
    base::RepeatingClosure action) {
  // Initialize layout if this is the first time a button is added.
  if (!selectable_profiles_container_->GetLayoutManager()) {
    selectable_profiles_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    // Give the container an accessible name so accessibility tools can provide
    // context for the buttons inside it. The role change is required by Windows
    // ATs.
    selectable_profiles_container_->GetViewAccessibility().OverrideName(
        profile_mgmt_heading_);
    selectable_profiles_container_->GetViewAccessibility().OverrideRole(
        ax::mojom::Role::kGroup);
  }

  DCHECK(!image_model.IsEmpty());
  gfx::ImageSkia sized_image = SizeImageModel(image_model, GetNativeTheme(),
                                              profiles::kMenuAvatarIconSize);

  views::Button* button = selectable_profiles_container_->AddChildView(
      std::make_unique<HoverButton>(
          base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                              base::Unretained(this), std::move(action)),
          sized_image, name));

  if (!is_guest && !first_profile_button_)
    first_profile_button_ = button;
}

void ProfileMenuViewBase::AddProfileManagementShortcutFeatureButton(
    const gfx::VectorIcon& icon,
    const base::string16& text,
    base::RepeatingClosure action) {
  // Initialize layout if this is the first time a button is added.
  if (!profile_mgmt_shortcut_features_container_->GetLayoutManager()) {
    profile_mgmt_shortcut_features_container_->SetLayoutManager(
        CreateBoxLayout(views::BoxLayout::Orientation::kHorizontal,
                        views::BoxLayout::CrossAxisAlignment::kCenter,
                        gfx::Insets(0, 0, 0, /*right=*/kMenuEdgeMargin)));
  }

  profile_mgmt_shortcut_features_container_->AddChildView(
      std::make_unique<CircularImageButton>(
          base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                              base::Unretained(this), std::move(action)),
          icon, text));
}

void ProfileMenuViewBase::AddProfileManagementFeatureButton(
    const gfx::VectorIcon& icon,
    const base::string16& text,
    base::RepeatingClosure action) {
  // Initialize layout if this is the first time a button is added.
  if (!profile_mgmt_features_container_->GetLayoutManager()) {
    profile_mgmt_features_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
  }

  auto icon_button = std::make_unique<ProfileManagementIconView>(icon);
  profile_mgmt_features_container_->AddChildView(std::make_unique<HoverButton>(
      base::BindRepeating(&ProfileMenuViewBase::ButtonPressed,
                          base::Unretained(this), std::move(action)),
      std::move(icon_button), text));
}

gfx::ImageSkia ProfileMenuViewBase::ColoredImageForMenu(
    const gfx::VectorIcon& icon,
    SkColor color) const {
  return gfx::CreateVectorIcon(icon, kMaxImageSize, color);
}

void ProfileMenuViewBase::RecordClick(ActionableItem item) {
  // TODO(tangltom): Separate metrics for incognito and guest menu.
  base::UmaHistogramEnumeration("Profile.Menu.ClickedActionableItem", item);
}

int ProfileMenuViewBase::GetMaxHeight() const {
  gfx::Rect anchor_rect = GetAnchorRect();
  gfx::Rect screen_space =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
          .work_area();
  int available_space = screen_space.bottom() - anchor_rect.bottom();
#if defined(OS_WIN)
  // On Windows the bubble can also be show to the top of the anchor.
  available_space =
      std::max(available_space, anchor_rect.y() - screen_space.y());
#endif
  return std::max(kMinimumScrollableContentHeight, available_space);
}

void ProfileMenuViewBase::Reset() {
  RemoveAllChildViews(/*delete_childen=*/true);

  auto components = std::make_unique<views::View>();
  components->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Create and add new component containers in the correct order.
  // First, add the parts of the current profile.
  heading_container_ =
      components->AddChildView(std::make_unique<views::View>());
  identity_info_container_ =
      components->AddChildView(std::make_unique<views::View>());
  shortcut_features_container_ =
      components->AddChildView(std::make_unique<views::View>());
  sync_info_container_ =
      components->AddChildView(std::make_unique<views::View>());
  features_container_ =
      components->AddChildView(std::make_unique<views::View>());
  profile_mgmt_separator_container_ =
      components->AddChildView(std::make_unique<views::View>());
  // Second, add the profile management header. This includes the heading and
  // the shortcut feature(s) next to it.
  auto profile_mgmt_header = std::make_unique<views::View>();
  views::BoxLayout* profile_mgmt_header_layout =
      profile_mgmt_header->SetLayoutManager(
          CreateBoxLayout(views::BoxLayout::Orientation::kHorizontal,
                          views::BoxLayout::CrossAxisAlignment::kCenter));
  profile_mgmt_heading_container_ =
      profile_mgmt_header->AddChildView(std::make_unique<views::View>());
  profile_mgmt_header_layout->SetFlexForView(profile_mgmt_heading_container_,
                                             1);
  profile_mgmt_shortcut_features_container_ =
      profile_mgmt_header->AddChildView(std::make_unique<views::View>());
  profile_mgmt_header_layout->SetFlexForView(
      profile_mgmt_shortcut_features_container_, 0);
  components->AddChildView(std::move(profile_mgmt_header));
  // Third, add the profile management buttons.
  selectable_profiles_container_ =
      components->AddChildView(std::make_unique<views::View>());
  profile_mgmt_features_container_ =
      components->AddChildView(std::make_unique<views::View>());
  first_profile_button_ = nullptr;

  // Create a scroll view to hold the components.
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  // TODO(https://crbug.com/871762): it's a workaround for the crash.
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->ClipHeightTo(0, GetMaxHeight());
  scroll_view->SetContents(std::move(components));

  // Create a grid layout to set the menu width.
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     views::GridLayout::kFixedSize,
                     views::GridLayout::ColumnSize::kFixed, kMenuWidth,
                     kMenuWidth);
  layout->StartRow(1.0f, 0);
  layout->AddView(std::move(scroll_view));
}

void ProfileMenuViewBase::FocusButtonOnKeyboardOpen() {
  if (first_profile_button_)
    first_profile_button_->RequestFocus();
}

void ProfileMenuViewBase::Init() {
  Reset();
  BuildMenu();
}

void ProfileMenuViewBase::OnWindowClosing() {
  DCHECK_EQ(g_profile_bubble_, this);
  if (anchor_button())
    anchor_button()->AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  g_profile_bubble_ = nullptr;
}

void ProfileMenuViewBase::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground)));
  UpdateSyncInfoContainerBackground();
}

ax::mojom::Role ProfileMenuViewBase::GetAccessibleWindowRole() {
  // Return |ax::mojom::Role::kMenuBar|, because it fits better the kind of UI
  // contained in this dialog. The top-level container in this dialog uses a
  // kMenu role to match.
  return ax::mojom::Role::kMenuBar;
}

bool ProfileMenuViewBase::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Suppresses the context menu because some features, such as inspecting
  // elements, are not appropriate in a bubble.
  return true;
}

void ProfileMenuViewBase::ButtonPressed(base::RepeatingClosure action) {
  DCHECK(action);
  signin_ui_util::RecordProfileMenuClick(browser()->profile());
  action.Run();
}

void ProfileMenuViewBase::UpdateSyncInfoContainerBackground() {
  ui::NativeTheme::ColorId bg_color;
  switch (sync_background_state_) {
    case SyncInfoContainerBackgroundState::kNoError:
      sync_info_container_->SetBackground(nullptr);
      return;
    case SyncInfoContainerBackgroundState::kPaused:
      bg_color = ui::NativeTheme::kColorId_SyncInfoContainerPaused;
      break;
    case SyncInfoContainerBackgroundState::kError:
      bg_color = ui::NativeTheme::kColorId_SyncInfoContainerError;
      break;
    case SyncInfoContainerBackgroundState::kNoPrimaryAccount:
      bg_color = ui::NativeTheme::kColorId_SyncInfoContainerNoPrimaryAccount;
  }
  sync_info_container_->SetBackground(views::CreateRoundedRectBackground(
      GetNativeTheme()->GetSystemColor(bg_color),
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::EMPHASIS_HIGH)));
}

// Despite ProfileMenuViewBase being a dialog, we are enforcing it to behave
// like a menu from the accessibility POV because it fits better with a menu UX.
// The dialog exposes the kMenuBar role, and the top-level container is kMenu.
// This class is responsible for emitting menu accessible events when the dialog
// is activated or deactivated.
class ProfileMenuViewBase::AXMenuWidgetObserver : public views::WidgetObserver {
 public:
  AXMenuWidgetObserver(ProfileMenuViewBase* owner, views::Widget* widget)
      : owner_(owner) {
    observation_.Observe(widget);
  }
  ~AXMenuWidgetObserver() override = default;

  void OnWidgetActivationChanged(views::Widget* widget, bool active) override {
    if (active) {
      owner_->NotifyAccessibilityEvent(ax::mojom::Event::kMenuStart, true);
      owner_->NotifyAccessibilityEvent(ax::mojom::Event::kMenuPopupStart, true);
    } else {
      owner_->NotifyAccessibilityEvent(ax::mojom::Event::kMenuPopupEnd, true);
      owner_->NotifyAccessibilityEvent(ax::mojom::Event::kMenuEnd, true);
    }
  }

 private:
  ProfileMenuViewBase* owner_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};
