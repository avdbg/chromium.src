// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"

#include <utility>

#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_ui_delegate.h"

TranslateBubbleModelImpl::TranslateBubbleModelImpl(
    translate::TranslateStep step,
    std::unique_ptr<translate::TranslateUIDelegate> ui_delegate)
    : ui_delegate_(std::move(ui_delegate)),
      view_state_transition_(TranslateStepToViewState(step)),
      translation_declined_(false),
      translate_executed_(false) {
  if (GetViewState() != TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE)
    translate_executed_ = true;
}

TranslateBubbleModelImpl::~TranslateBubbleModelImpl() {}

// static
TranslateBubbleModel::ViewState
TranslateBubbleModelImpl::TranslateStepToViewState(
    translate::TranslateStep step) {
  switch (step) {
    case translate::TRANSLATE_STEP_BEFORE_TRANSLATE:
      return TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE;
    case translate::TRANSLATE_STEP_TRANSLATING:
      return TranslateBubbleModel::VIEW_STATE_TRANSLATING;
    case translate::TRANSLATE_STEP_AFTER_TRANSLATE:
      return TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE;
    case translate::TRANSLATE_STEP_TRANSLATE_ERROR:
      return TranslateBubbleModel::VIEW_STATE_ERROR;
  }

  NOTREACHED();
  return TranslateBubbleModel::VIEW_STATE_ERROR;
}

TranslateBubbleModel::ViewState TranslateBubbleModelImpl::GetViewState() const {
  return view_state_transition_.view_state();
}

bool TranslateBubbleModelImpl::ShouldAlwaysTranslateBeCheckedByDefault() const {
  return ui_delegate_->ShouldAlwaysTranslateBeCheckedByDefault();
}

bool TranslateBubbleModelImpl::ShouldShowAlwaysTranslateShortcut() const {
  return ui_delegate_->ShouldShowAlwaysTranslateShortcut();
}

void TranslateBubbleModelImpl::SetViewState(
    TranslateBubbleModel::ViewState view_state) {
  view_state_transition_.SetViewState(view_state);
}

void TranslateBubbleModelImpl::ShowError(
    translate::TranslateErrors::Type error_type) {
  ui_delegate_->OnErrorShown(error_type);
}

void TranslateBubbleModelImpl::GoBackFromAdvanced() {
  view_state_transition_.GoBackFromAdvanced();
}

int TranslateBubbleModelImpl::GetNumberOfSourceLanguages() const {
  return ui_delegate_->GetNumberOfLanguages();
}

int TranslateBubbleModelImpl::GetNumberOfTargetLanguages() const {
  // Subtract 1 to account for unknown language option being omitted.
  return ui_delegate_->GetNumberOfLanguages() - 1;
}

base::string16 TranslateBubbleModelImpl::GetSourceLanguageNameAt(
    int index) const {
  return ui_delegate_->GetLanguageNameAt(index);
}

base::string16 TranslateBubbleModelImpl::GetTargetLanguageNameAt(
    int index) const {
  // Add 1 to account for unknown language option at index 0 in
  // TranslateUIDelegate language list.
  return ui_delegate_->GetLanguageNameAt(index + 1);
}

std::string TranslateBubbleModelImpl::GetOriginalLanguageCode() const {
  return ui_delegate_->GetOriginalLanguageCode();
}

int TranslateBubbleModelImpl::GetOriginalLanguageIndex() const {
  return ui_delegate_->GetOriginalLanguageIndex();
}

void TranslateBubbleModelImpl::UpdateOriginalLanguageIndex(int index) {
  ui_delegate_->UpdateOriginalLanguageIndex(index);
}

int TranslateBubbleModelImpl::GetTargetLanguageIndex() const {
  // Subtract 1 to account for unknown language option being omitted from the
  // bubble target language list.
  return ui_delegate_->GetTargetLanguageIndex() - 1;
}

void TranslateBubbleModelImpl::UpdateTargetLanguageIndex(int index) {
  // Add 1 to account for unknown language option at index 0 in
  // TranslateUIDelegate language list.
  ui_delegate_->UpdateTargetLanguageIndex(index + 1);
}

void TranslateBubbleModelImpl::DeclineTranslation() {
  translation_declined_ = true;
}

bool TranslateBubbleModelImpl::ShouldNeverTranslateLanguage() {
  return ui_delegate_->IsLanguageBlocked();
}

void TranslateBubbleModelImpl::SetNeverTranslateLanguage(bool value) {
  ui_delegate_->SetLanguageBlocked(value);
}

bool TranslateBubbleModelImpl::ShouldNeverTranslateSite() {
  return ui_delegate_->IsSiteOnNeverPromptList();
}

void TranslateBubbleModelImpl::SetNeverTranslateSite(bool value) {
  ui_delegate_->SetNeverPrompt(value);
}

bool TranslateBubbleModelImpl::CanBlocklistSite() {
  return ui_delegate_->CanAddToNeverPromptList();
}

bool TranslateBubbleModelImpl::ShouldAlwaysTranslate() const {
  return ui_delegate_->ShouldAlwaysTranslate();
}

void TranslateBubbleModelImpl::SetAlwaysTranslate(bool value) {
  ui_delegate_->SetAlwaysTranslate(value);
}

void TranslateBubbleModelImpl::Translate() {
  translate_executed_ = true;
  ui_delegate_->Translate();
}

void TranslateBubbleModelImpl::RevertTranslation() {
  ui_delegate_->RevertTranslation();
}

void TranslateBubbleModelImpl::OnBubbleClosing() {
  // TODO(curranmax): This will mark the UI as closed when the widget has lost
  // focus. This means it is basically impossible for the final state to have
  // the UI shown. https://crbug.com/1114868.
  ui_delegate_->OnUIClosedByUser();

  if (!translate_executed_)
    ui_delegate_->TranslationDeclined(translation_declined_);
}

bool TranslateBubbleModelImpl::IsPageTranslatedInCurrentLanguages() const {
  const translate::LanguageState& language_state =
      ui_delegate_->GetLanguageState();
  return ui_delegate_->GetOriginalLanguageCode() ==
             language_state.original_language() &&
         ui_delegate_->GetTargetLanguageCode() ==
             language_state.current_language();
}

void TranslateBubbleModelImpl::ReportUIInteraction(
    translate::UIInteraction ui_interaction) {
  ui_delegate_->ReportUIInteraction(ui_interaction);
}
