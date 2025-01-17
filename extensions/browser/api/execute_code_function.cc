// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_
#define EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_

#include "extensions/browser/api/execute_code_function.h"
#include "content/nw/src/browser/nw_chrome_browser_hooks.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/load_and_localize_file.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"

namespace {

// Error messages
const char kNoCodeOrFileToExecuteError[] = "No source code or file specified.";
const char kMoreThanOneValuesError[] =
    "Code and file should not be specified "
    "at the same time in the second argument.";
const char kBadFileEncodingError[] =
    "Could not load file '*' for content script. It isn't UTF-8 encoded.";
const char kLoadFileError[] = "Failed to load file: \"*\". ";
const char kCSSOriginForNonCSSError[] =
    "CSS origin should be specified only for CSS code.";

}

namespace extensions {

using api::extension_types::InjectDetails;

ExecuteCodeFunction::ExecuteCodeFunction() {
}

ExecuteCodeFunction::~ExecuteCodeFunction() {
}

void ExecuteCodeFunction::DidLoadAndLocalizeFile(
    const std::string& file,
    bool success,
    std::unique_ptr<std::string> data) {
  if (!success) {
    // TODO(viettrungluu): bug: there's no particular reason the path should be
    // UTF-8, in which case this may fail.
    Respond(Error(ErrorUtils::FormatErrorMessage(kLoadFileError, file)));
    return;
  }

  if (!base::IsStringUTF8(*data)) {
    Respond(Error(ErrorUtils::FormatErrorMessage(kBadFileEncodingError, file)));
    return;
  }

  std::string error;
  if (!Execute(*data, &error))
    Respond(Error(std::move(error)));

  // If Execute() succeeds, the function will respond in
  // OnExecuteCodeFinished().
}

bool ExecuteCodeFunction::Execute(const std::string& code_string,
                                  std::string* error) {
  ScriptExecutor* executor = GetScriptExecutor(error);
  if (!executor)
    return false;

  // TODO(lazyboy): Set |error|?
  if (!extension() && !IsWebView())
    return false;

  DCHECK(!(ShouldInsertCSS() && ShouldRemoveCSS()));

  auto action_type = UserScript::ActionType::ADD_JAVASCRIPT;
  if (ShouldInsertCSS())
    action_type = UserScript::ActionType::ADD_CSS;
  else if (ShouldRemoveCSS())
    action_type = UserScript::ActionType::REMOVE_CSS;

  ScriptExecutor::FrameScope frame_scope =
      details_->all_frames.get() && *details_->all_frames
          ? ScriptExecutor::INCLUDE_SUB_FRAMES
          : ScriptExecutor::SPECIFIED_FRAMES;

  root_frame_id_ = details_->frame_id.get()
                       ? *details_->frame_id
                       : ExtensionApiFrameIdMap::kTopFrameId;

  ScriptExecutor::MatchAboutBlank match_about_blank =
      details_->match_about_blank.get() && *details_->match_about_blank
          ? ScriptExecutor::MATCH_ABOUT_BLANK
          : ScriptExecutor::DONT_MATCH_ABOUT_BLANK;

  UserScript::RunLocation run_at = UserScript::UNDEFINED;
  switch (details_->run_at) {
    case api::extension_types::RUN_AT_NONE:
    case api::extension_types::RUN_AT_DOCUMENT_IDLE:
      run_at = UserScript::DOCUMENT_IDLE;
      break;
    case api::extension_types::RUN_AT_DOCUMENT_START:
      run_at = UserScript::DOCUMENT_START;
      break;
    case api::extension_types::RUN_AT_DOCUMENT_END:
      run_at = UserScript::DOCUMENT_END;
      break;
  }
  CHECK_NE(UserScript::UNDEFINED, run_at);

  CSSOrigin css_origin = CSSOrigin::kAuthor;
  switch (details_->css_origin) {
    case api::extension_types::CSS_ORIGIN_NONE:
    case api::extension_types::CSS_ORIGIN_AUTHOR:
      css_origin = CSSOrigin::kAuthor;
      break;
    case api::extension_types::CSS_ORIGIN_USER:
      css_origin = CSSOrigin::kUser;
      break;
  }

  ScriptExecutor::WorldType world_type = details_->main_world.get() && *details_->main_world
    ? ScriptExecutor::MAIN_WORLD : ScriptExecutor::ISOLATED_WORLD;

  executor->ExecuteScript(
      host_id_, action_type, code_string, frame_scope, {root_frame_id_},
      match_about_blank, run_at,
      world_type,
      IsWebView() ? ScriptExecutor::WEB_VIEW_PROCESS
                  : ScriptExecutor::DEFAULT_PROCESS,
      GetWebViewSrc(), script_url_, user_gesture(), css_origin,
      has_callback() ? ScriptExecutor::JSON_SERIALIZED_RESULT
                     : ScriptExecutor::NO_RESULT,
      base::BindOnce(&ExecuteCodeFunction::OnExecuteCodeFinished, this));
  return true;
}

ExtensionFunction::ResponseAction ExecuteCodeFunction::Run() {
  InitResult init_result = Init();
  EXTENSION_FUNCTION_VALIDATE(init_result != VALIDATION_FAILURE);
  if (init_result == FAILURE)
    return RespondNow(Error(init_error_.value_or(kUnknownErrorDoNotUse)));

  if (!details_->code && !details_->file)
    return RespondNow(Error(kNoCodeOrFileToExecuteError));

  if (details_->code && details_->file)
    return RespondNow(Error(kMoreThanOneValuesError));

  if (details_->css_origin != api::extension_types::CSS_ORIGIN_NONE &&
      !ShouldInsertCSS() && !ShouldRemoveCSS()) {
    return RespondNow(Error(kCSSOriginForNonCSSError));
  }

  std::string error;
  if (!CanExecuteScriptOnPage(&error))
    return RespondNow(Error(std::move(error)));

  if (details_->code) {
    if (!Execute(*details_->code, &error))
      return RespondNow(Error(std::move(error)));
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  DCHECK(details_->file);
  if (!LoadFile(*details_->file, &error))
    return RespondNow(Error(std::move(error)));

  // LoadFile will respond asynchronously later.
  return RespondLater();
}

bool ExecuteCodeFunction::LoadFile(const std::string& file,
                                   std::string* error) {
  ExtensionResource resource = extension()->GetResource(file);
  if (resource.extension_root().empty() || resource.relative_path().empty()) {
    *error = kNoCodeOrFileToExecuteError;
    return false;
  }
  script_url_ = extension()->GetResourceURL(file);

  bool might_require_localization = ShouldInsertCSS() || ShouldRemoveCSS();

  LoadAndLocalizeResource(
      *extension(), resource, might_require_localization,
      base::BindOnce(&ExecuteCodeFunction::DidLoadAndLocalizeFile, this,
                     resource.relative_path().AsUTF8Unsafe()));

  return true;
}

void ExecuteCodeFunction::OnExecuteCodeFinished(
    std::vector<ScriptExecutor::FrameResult> results) {
  DCHECK(!results.empty());

  auto root_frame_result =
      std::find_if(results.begin(), results.end(),
                   [root_frame_id = root_frame_id_](const auto& frame_result) {
                     return frame_result.frame_id == root_frame_id;
                   });

  DCHECK(root_frame_result != results.end());

  // We just error out if we never injected in the root frame.
  // TODO(devlin): That's a bit odd, because other injections may have
  // succeeded. It seems like it might be worth passing back the values
  // anyway.
  if (!root_frame_result->error.empty()) {
    // If the frame never responded (e.g. the frame was removed or didn't
    // exist), we provide a different error message for backwards
    // compatibility.
    if (!root_frame_result->frame_responded) {
      root_frame_result->error =
          root_frame_id_ == ExtensionApiFrameIdMap::kTopFrameId
              ? "The tab was closed."
              : "The frame was removed.";
    }

    Respond(Error(std::move(root_frame_result->error)));
    return;
  }

  if (ShouldInsertCSS() || ShouldRemoveCSS()) {
    // insertCSS and removeCSS don't have a result argument.
    Respond(NoArguments());
    return;
  }

  // Place the root frame result at the beginning.
  std::iter_swap(root_frame_result, results.begin());
  base::Value result_list(base::Value::Type::LIST);
  for (auto& result : results) {
    if (result.error.empty())
      result_list.Append(std::move(result.value));
  }

  Respond(OneArgument(std::move(result_list)));
}

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_
