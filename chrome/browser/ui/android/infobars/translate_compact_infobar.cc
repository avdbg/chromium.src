// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/translate_compact_infobar.h"

#include <stddef.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/bind.h"
#include "chrome/android/chrome_jni_headers/TranslateCompactInfoBar_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_context.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// Finch parameter names:
const char kTranslateTabDefaultTextColor[] = "translate_tab_default_text_color";

// ChromeTranslateClient
// ----------------------------------------------------------

std::unique_ptr<infobars::InfoBar> ChromeTranslateClient::CreateInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  return std::make_unique<TranslateCompactInfoBar>(std::move(delegate));
}

// TranslateInfoBar -----------------------------------------------------------

TranslateCompactInfoBar::TranslateCompactInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate)
    : infobars::InfoBarAndroid(std::move(delegate)), action_flags_(FLAG_NONE) {
  GetDelegate()->AddObserver(this);

  // Flip the translate bit if auto translate is enabled.
  if (GetDelegate()->translate_step() == translate::TRANSLATE_STEP_TRANSLATING)
    action_flags_ |= FLAG_TRANSLATE;
}

TranslateCompactInfoBar::~TranslateCompactInfoBar() {
  GetDelegate()->RemoveObserver(this);
}

ScopedJavaLocalRef<jobject> TranslateCompactInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();

  translate::JavaLanguageInfoWrapper translate_languages =
      translate::TranslateUtils::GetTranslateLanguagesInJavaFormat(env,
                                                                   delegate);
  // TODO(https://crbug.com/1173577): Refactor GetContentLanguagesInJavaFormat
  // to only return languageCodes.
  translate::JavaLanguageInfoWrapper content_languages =
      translate::TranslateUtils::GetContentLanguagesInJavaFormat(env, delegate);
  ScopedJavaLocalRef<jstring> source_language_code =
      base::android::ConvertUTF8ToJavaString(
          env, delegate->original_language_code());

  ScopedJavaLocalRef<jstring> target_language_code =
      base::android::ConvertUTF8ToJavaString(env,
                                             delegate->target_language_code());
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(this);

  TabAndroid* tab =
      web_contents ? TabAndroid::FromWebContents(web_contents) : nullptr;

  return Java_TranslateCompactInfoBar_create(
      env, tab ? tab->GetJavaObject() : nullptr, delegate->translate_step(),
      source_language_code, target_language_code,
      delegate->ShouldAlwaysTranslate(), delegate->triggered_from_menu(),
      translate_languages.java_languages, translate_languages.java_codes,
      translate_languages.java_hash_codes, content_languages.java_codes,
      TabDefaultTextColor());
}

void TranslateCompactInfoBar::ProcessButton(int action) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  if (action == infobars::InfoBarAndroid::ACTION_TRANSLATE) {
    action_flags_ |= FLAG_TRANSLATE;
    delegate->Translate();
    if (delegate->ShouldAutoAlwaysTranslate()) {
      JNIEnv* env = base::android::AttachCurrentThread();
      Java_TranslateCompactInfoBar_setAutoAlwaysTranslate(env,
                                                          GetJavaInfoBar());
    }
    delegate->ReportUIInteraction(translate::UIInteraction::kTranslate);
  } else if (action ==
             infobars::InfoBarAndroid::ACTION_TRANSLATE_SHOW_ORIGINAL) {
    action_flags_ |= FLAG_REVERT;
    delegate->RevertWithoutClosingInfobar();
    delegate->ReportUIInteraction(translate::UIInteraction::kRevert);
  } else {
    DCHECK_EQ(infobars::InfoBarAndroid::ACTION_NONE, action);
  }
}

void TranslateCompactInfoBar::SetJavaInfoBar(
    const base::android::JavaRef<jobject>& java_info_bar) {
  infobars::InfoBarAndroid::SetJavaInfoBar(java_info_bar);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TranslateCompactInfoBar_setNativePtr(env, java_info_bar,
                                            reinterpret_cast<intptr_t>(this));
}

void TranslateCompactInfoBar::ApplyStringTranslateOption(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    int option,
    const JavaParamRef<jstring>& value) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  if (option == translate::TranslateUtils::OPTION_SOURCE_CODE) {
    std::string source_code =
        base::android::ConvertJavaStringToUTF8(env, value);
    if (delegate->original_language_code().compare(source_code) != 0)
      delegate->UpdateOriginalLanguage(source_code);
    delegate->ReportUIInteraction(
        translate::UIInteraction::kChangeSourceLanguage);
  } else if (option == translate::TranslateUtils::OPTION_TARGET_CODE) {
    std::string target_code =
        base::android::ConvertJavaStringToUTF8(env, value);
    if (delegate->target_language_code().compare(target_code) != 0)
      delegate->UpdateTargetLanguage(target_code);
    delegate->ReportUIInteraction(
        translate::UIInteraction::kChangeTargetLanguage);
  } else {
    DCHECK(false);
  }
}

void TranslateCompactInfoBar::ApplyBoolTranslateOption(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    int option,
    jboolean value) {
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  if (option == translate::TranslateUtils::OPTION_ALWAYS_TRANSLATE) {
    if (delegate->ShouldAlwaysTranslate() != value) {
      action_flags_ |= FLAG_ALWAYS_TRANSLATE;
      delegate->ToggleAlwaysTranslate();
    }
    delegate->ReportUIInteraction(
        translate::UIInteraction::kAlwaysTranslateLanguage);
  } else if (option == translate::TranslateUtils::OPTION_NEVER_TRANSLATE) {
    if (value && delegate->IsTranslatableLanguageByPrefs()) {
      action_flags_ |= FLAG_NEVER_LANGUAGE;
      delegate->ToggleTranslatableLanguageByPrefs();
      RemoveSelf();
      delegate->OnInfoBarClosedByUser();
    }
    delegate->ReportUIInteraction(
        translate::UIInteraction::kNeverTranslateLanguage);
  } else if (option == translate::TranslateUtils::OPTION_NEVER_TRANSLATE_SITE) {
    if (value && !delegate->IsSiteOnNeverPromptList()) {
      action_flags_ |= FLAG_NEVER_SITE;
      delegate->ToggleNeverPrompt();
      RemoveSelf();
      delegate->OnInfoBarClosedByUser();
    }
    delegate->ReportUIInteraction(
        translate::UIInteraction::kNeverTranslateSite);
  } else {
    DCHECK(false);
  }
}

jboolean TranslateCompactInfoBar::ShouldAutoNeverTranslate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean menu_expanded) {
  // Flip menu expanded bit.
  if (menu_expanded)
    action_flags_ |= FLAG_EXPAND_MENU;

  if (!IsDeclinedByUser())
    return false;

  return GetDelegate()->ShouldAutoNeverTranslate();
}

// Returns true if the current tab is an incognito tab.
jboolean TranslateCompactInfoBar::IsIncognito(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(this);
  if (!web_contents)
    return false;
  return web_contents->GetBrowserContext()->IsOffTheRecord();
}

base::android::ScopedJavaLocalRef<jobjectArray>
TranslateCompactInfoBar::GetContentLanguagesCodes(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  std::vector<std::string> current_content_names;
  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  delegate->GetContentLanguagesCodes(&current_content_names);
  return base::android::ToJavaArrayOfStrings(env, current_content_names);
}

int TranslateCompactInfoBar::GetParam(const std::string& paramName,
                                      int default_value) {
  std::map<std::string, std::string> params;
  if (!variations::GetVariationParams(translate::kTranslateCompactUI.name,
                                      &params))
    return default_value;
  int value = 0;
  base::StringToInt(params[paramName], &value);
  return value <= 0 ? default_value : value;
}

int TranslateCompactInfoBar::TabDefaultTextColor() {
  return GetParam(kTranslateTabDefaultTextColor, 0);
}

translate::TranslateInfoBarDelegate* TranslateCompactInfoBar::GetDelegate() {
  return delegate()->AsTranslateInfoBarDelegate();
}

void TranslateCompactInfoBar::OnTranslateStepChanged(
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type) {
  // TODO(crbug/1093320): intended to mitigate a crash where
  // the java infobar is gone. If this works, look into root cause.
  if (!HasSetJavaInfoBar())
    return;  // No connected Java infobar

  if (!owner())
    return;  // We're closing; don't call anything.

  if ((step == translate::TRANSLATE_STEP_AFTER_TRANSLATE) ||
      (step == translate::TRANSLATE_STEP_TRANSLATE_ERROR)) {
    JNIEnv* env = base::android::AttachCurrentThread();
    bool error_ui_shown = Java_TranslateCompactInfoBar_onPageTranslated(
        env, GetJavaInfoBar(), error_type);

    if (error_ui_shown) {
      GetDelegate()->OnErrorShown(error_type);
    }
  } else if (step == translate::TRANSLATE_STEP_TRANSLATING) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_TranslateCompactInfoBar_onTranslating(env, GetJavaInfoBar());
  }
}

void TranslateCompactInfoBar::OnTargetLanguageChanged(
    const std::string& target_language_code) {
  // TODO(crbug/1093320): intended to mitigate a crash where
  // the java infobar is gone. If this works, look into root cause.
  if (!HasSetJavaInfoBar())
    return;  // No connected Java infobar

  if (!owner())
    return;  // We're closing; don't call anything.

  translate::TranslateInfoBarDelegate* delegate = GetDelegate();
  if (delegate->target_language_code().compare(target_language_code) == 0) {
    return;
  }
  delegate->UpdateTargetLanguage(target_language_code);

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> target_code =
      base::android::ConvertUTF8ToJavaString(env, target_language_code);
  Java_TranslateCompactInfoBar_onTargetLanguageChanged(env, GetJavaInfoBar(),
                                                       target_code);
}

bool TranslateCompactInfoBar::IsDeclinedByUser() {
  // Whether there is any affirmative action bit.
  return action_flags_ == FLAG_NONE;
}

void TranslateCompactInfoBar::OnTranslateInfoBarDelegateDestroyed(
    translate::TranslateInfoBarDelegate* delegate) {
  DCHECK_EQ(GetDelegate(), delegate);
  GetDelegate()->RemoveObserver(this);
}
