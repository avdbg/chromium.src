// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SCRIPT_EXECUTOR_H_
#define EXTENSIONS_BROWSER_SCRIPT_EXECUTOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "extensions/common/constants.h"
#include "extensions/common/user_script.h"

class GURL;
struct ExtensionMsg_ExecuteCode_Params;

namespace content {
class WebContents;
}

namespace extensions {

// Contains all extensions that are executing scripts, mapped to the paths for
// those scripts. The paths may be an empty set if the script has no path
// associated with it (e.g. in the case of tabs.executeScript), but there will
// still be an entry for the extension.
using ExecutingScriptsMap = std::map<std::string, std::set<std::string>>;

// Callback that ScriptExecutor uses to notify when content scripts and/or
// tabs.executeScript calls run on a page.
using ScriptsExecutedNotification = base::RepeatingCallback<
    void(content::WebContents*, const ExecutingScriptsMap&, const GURL&)>;

// Interface for executing extension content scripts (e.g. executeScript) as
// described by the ExtensionMsg_ExecuteCode_Params IPC, and notifying the
// caller when responded with ExtensionHostMsg_ExecuteCodeFinished.
class ScriptExecutor {
 public:
  explicit ScriptExecutor(content::WebContents* web_contents);
  ~ScriptExecutor();

  // The scope of the script injection across the frames.
  enum FrameScope {
    SPECIFIED_FRAMES,
    INCLUDE_SUB_FRAMES,
  };

  // Whether to insert the script in about: frames when its origin matches
  // the extension's host permissions.
  enum MatchAboutBlank {
    DONT_MATCH_ABOUT_BLANK,
    MATCH_ABOUT_BLANK,
  };

  // The type of world to inject into (main world, or its own isolated world).
  enum WorldType {
    MAIN_WORLD,
    ISOLATED_WORLD,
  };

  // The type of process the target is.
  enum ProcessType {
    DEFAULT_PROCESS,
    WEB_VIEW_PROCESS,
  };

  // The type of result the caller is interested in.
  enum ResultType {
    NO_RESULT,
    JSON_SERIALIZED_RESULT,
  };

  struct FrameResult {
    FrameResult();
    FrameResult(FrameResult&&);
    FrameResult& operator=(FrameResult&&);

    // The ID of the frame of the injection.
    int frame_id = -1;
    // The error associated with the injection, if any. Empty if the injection
    // succeeded.
    std::string error;
    // The URL of the frame from the injection. Only set if the frame exists.
    GURL url;
    // The result value from the injection, or null if the injection failed (or
    // had no result).
    base::Value value;
    // Whether the frame responded to the attempted injection (which can fail if
    // the frame was removed or never existed). Note this doesn't necessarily
    // mean the injection succeeded, since it could fail due to other reasons
    // (like permissions).
    bool frame_responded = false;
  };

  using ScriptFinishedCallback =
      base::OnceCallback<void(std::vector<FrameResult> frame_results)>;

  // Executes a script. The arguments match ExtensionMsg_ExecuteCode_Params in
  // extension_messages.h (request_id is populated automatically).
  //
  // The script will be executed in the frames identified by |frame_ids| (which
  // are extension API frame IDs). If |frame_scope| is INCLUDE_SUB_FRAMES,
  // then the script will also be executed in all descendants of the specified
  // frames.
  //
  // |callback| will always be called even if the IPC'd renderer is destroyed
  // before a response is received (in this case the callback will be with a
  // failure and appropriate error message).
  // TODO(devlin): Make |frame_ids| a std::set<> (since they must be unique).
  void ExecuteScript(const HostID& host_id,
                     UserScript::ActionType action_type,
                     const std::string& code,
                     FrameScope frame_scope,
                     const std::vector<int>& frame_ids,
                     MatchAboutBlank match_about_blank,
                     UserScript::RunLocation run_at,
                     WorldType world_type,
                     ProcessType process_type,
                     const GURL& webview_src,
                     const GURL& script_url,
                     bool user_gesture,
                     CSSOrigin css_origin,
                     ResultType result_type,
                     ScriptFinishedCallback callback);

  // Set the observer for ScriptsExecutedNotification callbacks.
  void set_observer(ScriptsExecutedNotification observer) {
    observer_ = std::move(observer);
  }

 private:
  // The next value to use for request_id in ExtensionMsg_ExecuteCode_Params.
  int next_request_id_ = 0;

  content::WebContents* web_contents_;

  ScriptsExecutedNotification observer_;

  DISALLOW_COPY_AND_ASSIGN(ScriptExecutor);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SCRIPT_EXECUTOR_H_
