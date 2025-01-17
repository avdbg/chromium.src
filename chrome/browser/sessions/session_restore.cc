// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_restore_delegate.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_log.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/sessions/tab_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/url_constants.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/sessions/core/session_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/ash_features.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#endif

using content::NavigationController;
using content::RenderWidgetHost;
using content::WebContents;
using RestoredTab = SessionRestoreDelegate::RestoredTab;

namespace {

bool HasSingleNewTabPage(Browser* browser) {
  if (browser->tab_strip_model()->count() != 1)
    return false;
  content::WebContents* active_tab =
      browser->tab_strip_model()->GetWebContentsAt(0);
  return active_tab->GetURL() == chrome::kChromeUINewTabURL ||
         search::IsInstantNTP(active_tab);
}

// Pointers to SessionRestoreImpls which are currently restoring the session.
std::set<SessionRestoreImpl*>* active_session_restorers = nullptr;

}  // namespace

// SessionRestoreImpl ---------------------------------------------------------

// SessionRestoreImpl is responsible for fetching the set of tabs to create
// from SessionService. SessionRestoreImpl deletes itself when done.

class SessionRestoreImpl : public BrowserListObserver {
 public:
  SessionRestoreImpl(Profile* profile,
                     Browser* browser,
                     bool synchronous,
                     bool clobber_existing_tab,
                     bool always_create_tabbed_browser,
                     bool log_event,
                     const std::vector<GURL>& urls_to_open,
                     SessionRestore::CallbackList* callbacks)
      : profile_(profile),
        browser_(browser),
        synchronous_(synchronous),
        clobber_existing_tab_(clobber_existing_tab),
        always_create_tabbed_browser_(always_create_tabbed_browser),
        log_event_(log_event),
        urls_to_open_(urls_to_open),
        active_window_id_(SessionID::InvalidValue()),
        restore_started_(base::TimeTicks::Now()),
        on_session_restored_callbacks_(callbacks) {
    if (active_session_restorers == nullptr)
      active_session_restorers = new std::set<SessionRestoreImpl*>();

    // Only one SessionRestoreImpl should be operating on the profile at the
    // same time.
    std::set<SessionRestoreImpl*>::const_iterator it;
    for (it = active_session_restorers->begin();
         it != active_session_restorers->end(); ++it) {
      if ((*it)->profile_ == profile)
        break;
    }
    DCHECK(it == active_session_restorers->end());

    active_session_restorers->insert(this);

    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
    profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kBrowserWindow);
  }

  bool synchronous() const { return synchronous_; }

  Browser* Restore() {
    SessionService* session_service =
        SessionServiceFactory::GetForProfile(profile_);
    DCHECK(session_service);
    session_service->GetLastSession(base::BindOnce(
        &SessionRestoreImpl::OnGotSession, weak_factory_.GetWeakPtr()));

    if (synchronous_) {
      {
        base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
        quit_closure_for_sync_restore_ = loop.QuitClosure();
        loop.Run();
        quit_closure_for_sync_restore_ = base::OnceClosure();
      }
      Browser* browser =
          ProcessSessionWindowsAndNotify(&windows_, active_window_id_);
      delete this;
      return browser;
    }

    if (browser_)
      BrowserList::AddObserver(this);

    return browser_;
  }

  // Restore window(s) from a foreign session. Returns newly created Browsers.
  std::vector<Browser*> RestoreForeignSession(
      std::vector<const sessions::SessionWindow*>::const_iterator begin,
      std::vector<const sessions::SessionWindow*>::const_iterator end) {
    std::vector<Browser*> browsers;
    std::vector<RestoredTab> created_contents;
    // Create a browser instance to put the restored tabs in.
    for (auto i = begin; i != end; ++i) {
      Browser* browser = CreateRestoredBrowser(
          BrowserTypeForWindowType((*i)->type), (*i)->bounds, (*i)->workspace,
          (*i)->visible_on_all_workspaces, (*i)->show_state, (*i)->app_name,
          (*i)->user_title, (*i)->window_id.id());
      browsers.push_back(browser);

      // Restore and show the browser.
      const int initial_tab_count = 0;
      RestoreTabsToBrowser(*(*i), browser, initial_tab_count,
                           &created_contents);
      NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);
    }

    // Always create in a new window.
    FinishedTabCreation(true, true, &created_contents);

    on_session_restored_callbacks_->Notify(
        static_cast<int>(created_contents.size()));

    return browsers;
  }

  // Restore a single tab from a foreign session.
  // Opens in the tab in the last active browser, unless disposition is
  // NEW_WINDOW, in which case the tab will be opened in a new browser. Returns
  // the WebContents of the restored tab.
  WebContents* RestoreForeignTab(const sessions::SessionTab& tab,
                                 WindowOpenDisposition disposition) {
    DCHECK(!tab.navigations.empty());
    int selected_index = tab.current_navigation_index;
    selected_index = std::max(
        0,
        std::min(selected_index, static_cast<int>(tab.navigations.size() - 1)));

    bool use_new_window = disposition == WindowOpenDisposition::NEW_WINDOW;

    Browser* browser =
        use_new_window ? Browser::Create(Browser::CreateParams(profile_, true))
                       : browser_;

    RecordAppLaunchForTab(browser, tab, selected_index);

    WebContents* web_contents;
    if (disposition == WindowOpenDisposition::CURRENT_TAB) {
      DCHECK(!use_new_window);
      web_contents = chrome::ReplaceRestoredTab(
          browser, tab.navigations, selected_index, tab.extension_app_id,
          nullptr, tab.user_agent_override, true /* from_session_restore */);
    } else {
      int tab_index =
          use_new_window ? 0 : browser->tab_strip_model()->active_index() + 1;
      web_contents = chrome::AddRestoredTab(
          browser, tab.navigations, tab_index, selected_index,
          tab.extension_app_id, base::nullopt,
          disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB,  // selected
          tab.pinned, base::TimeTicks(), nullptr, tab.user_agent_override,
          true /* from_session_restore */);
      // Start loading the tab immediately.
      web_contents->GetController().LoadIfNecessary();
    }

    if (use_new_window) {
      browser->tab_strip_model()->ActivateTabAt(
          0, {TabStripModel::GestureType::kOther});
      browser->window()->Show();
    }
    NotifySessionServiceOfRestoredTabs(browser,
                                       browser->tab_strip_model()->count());

    // Since FinishedTabCreation() is not called here, |this| will leak if we
    // are not in sychronous mode.
    DCHECK(synchronous_);

    on_session_restored_callbacks_->Notify(1);

    return web_contents;
  }

  ~SessionRestoreImpl() override {
    BrowserList::RemoveObserver(this);
    active_session_restorers->erase(this);
    if (active_session_restorers->empty()) {
      delete active_session_restorers;
      active_session_restorers = nullptr;
    }
  }

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override {
    if (browser == browser_)
      delete this;
  }

  Profile* profile() { return profile_; }

 private:
  // Invoked when done with creating all the tabs/browsers.
  //
  // |created_tabbed_browser| indicates whether a tabbed browser was created,
  // or we used an existing tabbed browser.
  //
  // If successful, this begins loading tabs and deletes itself when all tabs
  // have been loaded.
  //
  // Returns the Browser that was created, if any.
  Browser* FinishedTabCreation(bool succeeded,
                               bool created_tabbed_browser,
                               std::vector<RestoredTab>* contents_created) {
    Browser* browser = nullptr;
    if (!created_tabbed_browser && always_create_tabbed_browser_) {
      browser = Browser::Create(Browser::CreateParams(profile_, false));
      if (urls_to_open_.empty()) {
        // No tab browsers were created and no URLs were supplied on the command
        // line. Open the new tab page.
        urls_to_open_.push_back(GURL(chrome::kChromeUINewTabURL));
      }
      AppendURLsToBrowser(browser, urls_to_open_);
      browser->window()->Show();
    }

    if (succeeded) {
      // Sort the tabs in the order they should be restored, and start loading
      // them.
      std::stable_sort(contents_created->begin(), contents_created->end());
      SessionRestoreDelegate::RestoreTabs(*contents_created, restore_started_);
    }

    if (!synchronous_) {
      // If we're not synchronous we need to delete ourself.
      // NOTE: we must use DeleteLater here as most likely we're in a callback
      // from the history service which doesn't deal well with deleting the
      // object it is notifying.
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);

      // The delete may take a while and at this point we no longer care about
      // if the browser is deleted. Don't listen to anything. This avoid a
      // possible double delete too (if browser is closed before DeleteSoon() is
      // processed).
      BrowserList::RemoveObserver(this);
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker("SessionRestore-End",
                                                           false);
#endif
    return browser;
  }

  void OnGotSession(
      std::vector<std::unique_ptr<sessions::SessionWindow>> windows,
      SessionID active_window_id,
      bool read_error) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker(
        "SessionRestore-GotSession", false);
#endif
    read_error_ = read_error;
    if (synchronous_) {
      // See comment above windows_ as to why we don't process immediately.
      windows_.swap(windows);
      active_window_id_ = active_window_id;
      CHECK(!quit_closure_for_sync_restore_.is_null());
      std::move(quit_closure_for_sync_restore_).Run();
      return;
    }

    ProcessSessionWindowsAndNotify(&windows, active_window_id);
  }

  Browser* ProcessSessionWindowsAndNotify(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
      SessionID active_window_id) {
    int window_count = 0;
    int tab_count = 0;
    std::vector<RestoredTab> contents;
    Browser* result = ProcessSessionWindows(
        windows, active_window_id, &contents, &window_count, &tab_count);
    if (log_event_) {
      LogSessionServiceRestoreEvent(profile_, window_count, tab_count,
                                    read_error_);
    }
    on_session_restored_callbacks_->Notify(static_cast<int>(contents.size()));
    return result;
  }

  Browser* ProcessSessionWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
      SessionID active_window_id,
      std::vector<RestoredTab>* created_contents,
      int* window_count,
      int* tab_count) {
    DVLOG(1) << "ProcessSessionWindows " << windows->size();

    if (windows->empty()) {
      // Restore was unsuccessful. The DOM storage system can also delete its
      // data, since no session restore will happen at a later point in time.
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetDOMStorageContext()
          ->StartScavengingUnusedSessionStorage();
      return FinishedTabCreation(false, false, created_contents);
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker(
        "SessionRestore-CreatingTabs-Start", false);
#endif

    // After the for loop this contains the last TYPE_NORMAL browser, or nullptr
    // if no TYPE_NORMAL browser exists.
    Browser* last_normal_browser = nullptr;
    bool has_normal_browser = false;

    // After the for loop, this contains the browser to activate, if one of the
    // windows has the same id as specified in active_window_id.
    Browser* browser_to_activate = nullptr;

    // Determine if there is a visible window, or if the active window exists.
    // Even if all windows are ui::SHOW_STATE_MINIMIZED, if one of them is the
    // active window it will be made visible by the call to
    // browser_to_activate->window()->Activate() later on in this method.
    bool has_visible_browser = false;
    for (const auto& window : *windows) {
      if (window->show_state != ui::SHOW_STATE_MINIMIZED ||
          window->window_id == active_window_id)
        has_visible_browser = true;
    }

    for (auto i = windows->begin(); i != windows->end(); ++i) {
      ++(*window_count);
      // 1. Choose between restoring tabs in an existing browser or in a newly
      //    created browser.
      Browser* browser = nullptr;
      if (i == windows->begin() &&
          (*i)->type == sessions::SessionWindow::TYPE_NORMAL && browser_ &&
          browser_->is_type_normal() &&
          !browser_->profile()->IsOffTheRecord()) {
        // The first set of tabs is added to the existing browser.
        browser = browser_;
      } else {
#if BUILDFLAG(IS_CHROMEOS_ASH)
        chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker(
            "SessionRestore-CreateRestoredBrowser-Start", false);
#endif
        // Change the initial show state of the created browser to
        // SHOW_STATE_NORMAL if there are no visible browsers.
        ui::WindowShowState show_state = (*i)->show_state;
        if (!has_visible_browser) {
          show_state = ui::SHOW_STATE_NORMAL;
          has_visible_browser = true;
        }
        browser = CreateRestoredBrowser(
            BrowserTypeForWindowType((*i)->type), (*i)->bounds, (*i)->workspace,
            (*i)->visible_on_all_workspaces, show_state, (*i)->app_name,
            (*i)->user_title, (*i)->window_id.id());
#if BUILDFLAG(IS_CHROMEOS_ASH)
        chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker(
            "SessionRestore-CreateRestoredBrowser-End", false);
#endif
      }

      // 2. Track TYPE_NORMAL browsers.
      if ((*i)->type == sessions::SessionWindow::TYPE_NORMAL) {
        has_normal_browser = true;
        last_normal_browser = browser;
      }

      // 3. Determine whether the currently active tab should be closed.
      WebContents* active_tab =
          browser->tab_strip_model()->GetActiveWebContents();
      int initial_tab_count = browser->tab_strip_model()->count();
      bool close_active_tab =
          clobber_existing_tab_ && i == windows->begin() &&
          (*i)->type == sessions::SessionWindow::TYPE_NORMAL && active_tab &&
          browser == browser_ && !(*i)->tabs.empty();
      if (close_active_tab)
        --initial_tab_count;
      if ((*i)->window_id == active_window_id)
        browser_to_activate = browser;

      // 5. Restore tabs in |browser|. This will also call Show() on |browser|
      //    if its initial show state is not mimimized.
      // However, with desks restore enabled, a window is restored to its parent
      // desk, which can be non-active desk, and left invisible but unminimized.
      RestoreTabsToBrowser(*(*i), browser, initial_tab_count, created_contents);
      (*tab_count) += (static_cast<int>(browser->tab_strip_model()->count()) -
                       initial_tab_count);
#if BUILDFLAG(IS_CHROMEOS_ASH)
      DCHECK(browser->window()->IsVisible() ||
             browser->window()->IsMinimized() ||
             ash::features::IsBentoEnabled());
#else
      DCHECK(browser->window()->IsVisible() ||
             browser->window()->IsMinimized());
#endif

      // 6. Tabs will be grouped appropriately in RestoreTabsToBrowser. Now
      //    restore the groups' visual data.
      TabGroupModel* group_model = browser->tab_strip_model()->group_model();
      for (auto& session_tab_group : (*i)->tab_groups) {
        TabGroup* model_tab_group =
            group_model->GetTabGroup(session_tab_group->id);
        DCHECK(model_tab_group);
        model_tab_group->SetVisualData(session_tab_group->visual_data);
      }

      // 7. Notify SessionService of restored tabs, so they can be saved to the
      //    current session.
      // TODO(fdoray): This seems redundant with the call to
      // SessionService::TabRestored() at the end of chrome::AddRestoredTab().
      // Consider removing it.
      NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);

      // 8. Close the tab that was active in the window prior to session
      //    restore, if needed.
      if (close_active_tab)
        chrome::CloseWebContents(browser, active_tab, true);

      // Sanity check: A restored browser should have an active tab.
      // TODO(https://crbug.com/1032348): Change to DCHECK once we understand
      // why some browsers don't have an active tab on startup.
      CHECK(browser->tab_strip_model()->GetActiveWebContents());
    }

    if (browser_to_activate && browser_to_activate->is_type_normal())
      last_normal_browser = browser_to_activate;

    if (last_normal_browser && !urls_to_open_.empty())
      AppendURLsToBrowser(last_normal_browser, urls_to_open_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker(
        "SessionRestore-CreatingTabs-End", false);
#endif
    if (browser_to_activate)
      browser_to_activate->window()->Activate();

    // If last_normal_browser is NULL and urls_to_open_ is non-empty,
    // FinishedTabCreation will create a new TabbedBrowser and add the urls to
    // it.
    Browser* finished_browser =
        FinishedTabCreation(true, has_normal_browser, created_contents);
    if (finished_browser)
      last_normal_browser = finished_browser;

    // sessionStorages needed for the session restore have now been recreated
    // by RestoreTab. Now it's safe for the DOM storage system to start
    // deleting leftover data.
    content::BrowserContext::GetDefaultStoragePartition(profile_)
        ->GetDOMStorageContext()
        ->StartScavengingUnusedSessionStorage();
    return last_normal_browser;
  }

  // Record an app launch event (if appropriate) for a tab which is about to
  // be restored. Callers should ensure that selected_index is within the
  // bounds of tab.navigations before calling.
  void RecordAppLaunchForTab(Browser* browser,
                             const sessions::SessionTab& tab,
                             int selected_index) {
    DCHECK(selected_index >= 0 &&
           selected_index < static_cast<int>(tab.navigations.size()));
    GURL url = tab.navigations[selected_index].virtual_url();
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile())
            ->enabled_extensions()
            .GetAppByURL(url);
    if (extension) {
      extensions::RecordAppLaunchType(
          extension_misc::APP_LAUNCH_SESSION_RESTORE, extension->GetType());
    }
  }

  // Adds the tabs from |window| to |browser|. Normal tabs go after the existing
  // tabs but pinned tabs will be pushed in front.
  // If there are no existing tabs, the tab at |window.selected_tab_index| will
  // be selected. Otherwise, the tab selection will remain untouched.
  void RestoreTabsToBrowser(const sessions::SessionWindow& window,
                            Browser* browser,
                            int initial_tab_count,
                            std::vector<RestoredTab>* created_contents) {
    DVLOG(1) << "RestoreTabsToBrowser " << window.tabs.size();
    // TODO(https://crbug.com/1032348): Change to DCHECK once we understand
    // why some browsers don't have an active tab on startup.
    CHECK(!window.tabs.empty());
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeTicks latest_last_active_time = base::TimeTicks::UnixEpoch();
    // The last active time of a WebContents is initially set to the
    // creation time of the tab, which is not necessarly the same as the
    // loading time, so we have to restore the values. Also, since TimeTicks
    // only make sense in their current session, these values have to be
    // sanitized first. To do so, we need to first figure out the largest
    // time. This will then be used to set the last active time of
    // each tab where the most recent tab will have its time set to |now|
    // and the rest of the tabs will have theirs set earlier by the same
    // delta as they originally had.
    for (int i = 0; i < static_cast<int>(window.tabs.size()); ++i) {
      const sessions::SessionTab& tab = *(window.tabs[i]);
      if (tab.last_active_time > latest_last_active_time)
        latest_last_active_time = tab.last_active_time;
    }

    // TODO(crbug.com/930991): Check that tab groups are contiguous in |window|
    // to ensure tabs will not be reordered when restoring. This is not possible
    // yet due the ordering of TabStripModelObserver notifications in an edge
    // case.

    const int selected_tab_index = base::ClampToRange(
        window.selected_tab_index, 0, static_cast<int>(window.tabs.size() - 1));

    for (int i = 0; i < static_cast<int>(window.tabs.size()); ++i) {
      const sessions::SessionTab& tab = *(window.tabs[i]);

      // Loads are scheduled for each restored tab unless the tab is going to
      // be selected as ShowBrowser() will load the selected tab.
      bool is_selected_tab =
          (initial_tab_count == 0) && (i == selected_tab_index);

      // Sanitize the last active time.
      base::TimeDelta delta = latest_last_active_time - tab.last_active_time;
      base::TimeTicks last_active_time = now - delta;

      // If the browser already has tabs, we want to restore the new ones after
      // the existing ones. E.g. this happens in Win8 Metro where we merge
      // windows or when launching a hosted app from the app launcher.
      int tab_index = i + initial_tab_count;
      RestoreTab(tab, browser, created_contents, tab_index, is_selected_tab,
                 last_active_time);
    }
  }

  // |tab_index| is ignored for pinned tabs which will always be pushed behind
  // the last existing pinned tab.
  // |tab_loader_| will schedule this tab for loading if |is_selected_tab| is
  // false. |last_active_time| is the value to use to set the last time the
  // WebContents was made active.
  void RestoreTab(const sessions::SessionTab& tab,
                  Browser* browser,
                  std::vector<RestoredTab>* created_contents,
                  const int tab_index,
                  bool is_selected_tab,
                  base::TimeTicks last_active_time) {
    // It's possible (particularly for foreign sessions) to receive a tab
    // without valid navigations. In that case, just skip it.
    // See crbug.com/154129.
    if (tab.navigations.empty())
      return;

    SessionRestore::NotifySessionRestoreStartedLoadingTabs();
    int selected_index = GetNavigationIndexToSelect(tab);

    RecordAppLaunchForTab(browser, tab, selected_index);

    // Associate sessionStorage (if any) to the restored tab.
    scoped_refptr<content::SessionStorageNamespace> session_storage_namespace;
    if (!tab.session_storage_persistent_id.empty()) {
      session_storage_namespace =
          content::BrowserContext::GetDefaultStoragePartition(profile_)
              ->GetDOMStorageContext()
              ->RecreateSessionStorage(tab.session_storage_persistent_id);
    }

    // Apply the stored group.
    WebContents* web_contents = chrome::AddRestoredTab(
        browser, tab.navigations, tab_index, selected_index,
        tab.extension_app_id, tab.group, is_selected_tab, tab.pinned,
        last_active_time, session_storage_namespace.get(),
        tab.user_agent_override, true /* from_session_restore */);
    DCHECK(web_contents);

    RestoredTab restored_tab(web_contents, is_selected_tab,
                             tab.extension_app_id.empty(), tab.pinned,
                             tab.group);
    created_contents->push_back(restored_tab);

    // If this isn't the selected tab, there's nothing else to do.
    if (!is_selected_tab)
      return;

    ShowBrowser(browser, browser->tab_strip_model()->GetIndexOfWebContents(
                             web_contents));
  }

  Browser* CreateRestoredBrowser(Browser::Type type,
                                 gfx::Rect bounds,
                                 const std::string& workspace,
                                 bool visible_on_all_workspaces,
                                 ui::WindowShowState show_state,
                                 const std::string& app_name,
                                 const std::string& user_title,
                                 int32_t restore_id) {
    Browser::CreateParams params(type, profile_, false);
    params.initial_bounds = bounds;
    params.user_title = user_title;

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // We only store trusted app windows, so we also create them as trusted.
    if (type == Browser::Type::TYPE_APP) {
      params = Browser::CreateParams::CreateForApp(
          app_name, /*trusted_source=*/true, bounds, profile_,
          /*user_gesture=*/false);
    } else if (type == Browser::Type::TYPE_APP_POPUP) {
      params = Browser::CreateParams::CreateForAppPopup(
          app_name, /*trusted_source=*/true, bounds, profile_,
          /*user_gesture=*/false);
    }
    params.restore_id = restore_id;
#endif

    params.initial_show_state = show_state;
    params.initial_workspace = workspace;
    params.initial_visible_on_all_workspaces_state = visible_on_all_workspaces;
    params.is_session_restore = true;
    return Browser::Create(params);
  }

  void ShowBrowser(Browser* browser, int selected_tab_index) {
    DCHECK(browser);
    DCHECK(browser->tab_strip_model()->count());
    browser->tab_strip_model()->ActivateTabAt(
        selected_tab_index, {TabStripModel::GestureType::kOther});

    if (browser_ == browser)
      return;

    browser->window()->Show();
    browser->set_is_session_restore(false);
  }

  // Appends the urls in |urls| to |browser|.
  void AppendURLsToBrowser(Browser* browser, const std::vector<GURL>& urls) {
    for (size_t i = 0; i < urls.size(); ++i) {
      int add_types = TabStripModel::ADD_FORCE_INDEX;
      if (i == 0)
        add_types |= TabStripModel::ADD_ACTIVE;
      NavigateParams params(browser, urls[i],
                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
      params.disposition = i == 0 ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                  : WindowOpenDisposition::NEW_BACKGROUND_TAB;
      params.tabstrip_add_types = add_types;
      Navigate(&params);
    }
  }

  // Invokes TabRestored on the SessionService for all tabs in browser after
  // initial_count.
  void NotifySessionServiceOfRestoredTabs(Browser* browser, int initial_count) {
    SessionService* session_service =
        SessionServiceFactory::GetForProfile(profile_);
    if (!session_service)
      return;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int i = initial_count; i < tab_strip->count(); ++i) {
      session_service->TabRestored(tab_strip->GetWebContentsAt(i),
                                   tab_strip->IsTabPinned(i));
    }
  }

  // The profile to create the sessions for.
  Profile* profile_;

  // The first browser to restore to, may be null.
  Browser* browser_;

  // Whether or not restore is synchronous.
  const bool synchronous_;

  // The quit-closure to terminate the nested message-loop started for
  // synchronous session-restore.
  base::OnceClosure quit_closure_for_sync_restore_;

  // See description of CLOBBER_CURRENT_TAB.
  const bool clobber_existing_tab_;

  // If true and there is an error or there are no windows to restore, we
  // create a tabbed browser anyway. This is used on startup to make sure at
  // at least one window is created.
  const bool always_create_tabbed_browser_;

  // If true, LogSessionServiceRestoreEvent() is called after restore.
  const bool log_event_;

  // Set of URLs to open in addition to those restored from the session.
  std::vector<GURL> urls_to_open_;

  // Responsible for loading the tabs.
  scoped_refptr<TabLoader> tab_loader_;

  // When synchronous we run a nested run loop. To avoid creating windows
  // from the nested run loop (which can make exiting the nested message
  // loop take a while) we cache the SessionWindows here and create the actual
  // windows when the nested run loop exits.
  std::vector<std::unique_ptr<sessions::SessionWindow>> windows_;
  SessionID active_window_id_;

  // When asynchronous it's possible for there to be no windows. To make sure
  // Chrome doesn't prematurely exit we register a KeepAlive for the lifetime
  // of this object.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // Same as |keep_alive_|, but also prevent |profile_| from getting deleted
  // (when DestroyProfileOnBrowserClose is enabled).
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  // The time we started the restore.
  base::TimeTicks restore_started_;

  // List of callbacks for session restore notification.
  SessionRestore::CallbackList* on_session_restored_callbacks_;

  // Set to true if reading the last commands encountered an error.
  bool read_error_ = false;

  base::WeakPtrFactory<SessionRestoreImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SessionRestoreImpl);
};

// SessionRestore -------------------------------------------------------------

// static
Browser* SessionRestore::RestoreSession(
    Profile* profile, Browser* browser,
    SessionRestore::BehaviorBitmask behavior,
    const std::vector<GURL>& urls_to_open) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker(
      "SessionRestore-Start", false);
#endif
  DCHECK(profile);
  DCHECK(SessionServiceFactory::GetForProfile(profile));
  profile->set_restored_last_session(true);
  // SessionRestoreImpl takes care of deleting itself when done.
  SessionRestoreImpl* restorer =
      new SessionRestoreImpl(profile, browser, (behavior & SYNCHRONOUS) != 0,
                             (behavior & CLOBBER_CURRENT_TAB) != 0,
                             (behavior & ALWAYS_CREATE_TABBED_BROWSER) != 0,
                             /* log_event */ true, urls_to_open,
                             SessionRestore::on_session_restored_callbacks());
  return restorer->Restore();
}

// static
void SessionRestore::RestoreSessionAfterCrash(Browser* browser) {
  auto* profile = browser->profile();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Bento restores a window to the right desk, so we should not
  // reuse any browser window. Otherwise, the conflict of the parent desk
  // arises because tabs created in this |browser| should remain in the
  // current active desk, but the first restored window should be restored
  // to its saved parent desk before a crash. This also avoids users'
  // confusion of the current window disappearing from the current desk
  // after pressing a restore button.
  if (ash::features::IsBentoEnabled())
    browser = nullptr;
#endif

  SessionRestore::BehaviorBitmask behavior =
      browser && HasSingleNewTabPage(browser)
          ? SessionRestore::CLOBBER_CURRENT_TAB
          : 0;

  SessionRestore::RestoreSession(profile, browser, behavior,
                                 std::vector<GURL>());
}

// static
void SessionRestore::OpenStartupPagesAfterCrash(Browser* browser) {
  WebContents* tab_to_clobber = nullptr;
  if (HasSingleNewTabPage(browser))
    tab_to_clobber = browser->tab_strip_model()->GetActiveWebContents();

  StartupBrowserCreator::OpenStartupPages(browser, true);
  if (tab_to_clobber && browser->tab_strip_model()->count() > 1)
    chrome::CloseWebContents(browser, tab_to_clobber, true);
}

// static
std::vector<Browser*> SessionRestore::RestoreForeignSessionWindows(
    Profile* profile,
    std::vector<const sessions::SessionWindow*>::const_iterator begin,
    std::vector<const sessions::SessionWindow*>::const_iterator end) {
  std::vector<GURL> gurls;
  SessionRestoreImpl restorer(profile, static_cast<Browser*>(nullptr), true,
                              false, true, /* log_event */ false, gurls,
                              on_session_restored_callbacks());
  return restorer.RestoreForeignSession(begin, end);
}

// static
WebContents* SessionRestore::RestoreForeignSessionTab(
    content::WebContents* source_web_contents,
    const sessions::SessionTab& tab,
    WindowOpenDisposition disposition) {
  Browser* browser = chrome::FindBrowserWithWebContents(source_web_contents);
  Profile* profile = browser->profile();
  std::vector<GURL> gurls;
  SessionRestoreImpl restorer(profile, browser, true, false, false,
                              /* log_event */ false, gurls,
                              on_session_restored_callbacks());
  return restorer.RestoreForeignTab(tab, disposition);
}

// static
bool SessionRestore::IsRestoring(const Profile* profile) {
  if (active_session_restorers == nullptr)
    return false;
  for (auto it = active_session_restorers->begin();
       it != active_session_restorers->end(); ++it) {
    if ((*it)->profile() == profile)
      return true;
  }
  return false;
}

// static
bool SessionRestore::IsRestoringSynchronously() {
  if (!active_session_restorers)
    return false;
  for (auto it = active_session_restorers->begin();
       it != active_session_restorers->end(); ++it) {
    if ((*it)->synchronous())
      return true;
  }
  return false;
}

// static
base::CallbackListSubscription
SessionRestore::RegisterOnSessionRestoredCallback(
    const base::RepeatingCallback<void(int)>& callback) {
  return on_session_restored_callbacks()->Add(callback);
}

// static
void SessionRestore::AddObserver(SessionRestoreObserver* observer) {
  observers()->AddObserver(observer);
}

// static
void SessionRestore::RemoveObserver(SessionRestoreObserver* observer) {
  observers()->RemoveObserver(observer);
}

// static
void SessionRestore::OnTabLoaderFinishedLoadingTabs() {
  if (!session_restore_started_)
    return;

  session_restore_started_ = false;
  for (auto& observer : *observers())
    observer.OnSessionRestoreFinishedLoadingTabs();
}

// static
void SessionRestore::NotifySessionRestoreStartedLoadingTabs() {
  if (session_restore_started_)
    return;

  session_restore_started_ = true;
  for (auto& observer : *observers())
    observer.OnSessionRestoreStartedLoadingTabs();
}

// static
void SessionRestore::OnWillRestoreTab(content::WebContents* web_contents) {
  for (auto& observer : *observers())
    observer.OnWillRestoreTab(web_contents);
}

// static
base::CallbackList<void(int)>*
    SessionRestore::on_session_restored_callbacks_ = nullptr;

// static
base::ObserverList<SessionRestoreObserver>::Unchecked*
    SessionRestore::observers_ = nullptr;

// static
bool SessionRestore::session_restore_started_ = false;
