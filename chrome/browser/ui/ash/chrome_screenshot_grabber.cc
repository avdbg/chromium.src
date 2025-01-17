// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/shell.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber_test_observer.h"
#include "chrome/browser/ui/ash/clipboard_util.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

using ui::ScreenshotResult;

const char kNotificationId[] = "screenshot";
const char kNotifierScreenshot[] = "ash.screenshot";

const char kNotificationOriginUrl[] = "chrome://screenshot";

// User is waiting for the screenshot-taken notification, hence USER_VISIBLE.
constexpr base::TaskTraits kBlockingTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

ChromeScreenshotGrabber* g_chrome_screenshot_grabber_instance = nullptr;

// Delegate for a notification. This class has two roles: to implement callback
// methods for notification, and to provide an identity of the associated
// notification.
class ScreenshotGrabberNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  ScreenshotGrabberNotificationDelegate(bool success,
                                        Profile* profile,
                                        const base::FilePath& screenshot_path)
      : success_(success),
        profile_(profile),
        screenshot_path_(screenshot_path) {}

  // message_center::NotificationDelegate:
  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    if (!button_index) {
      // TODO(estade): this conditional can be a DCHECK after
      // NotificationDelegate::Click() is not called for notifications that are
      // not set clickable.
      if (success_) {
        platform_util::ShowItemInFolder(profile_, screenshot_path_);
        NotificationDisplayService::GetForProfile(profile_)->Close(
            NotificationHandler::Type::TRANSIENT, kNotificationId);
        base::RecordAction(base::UserMetricsAction("Screenshot_ShowInFolder"));
      }
      return;
    }
    DCHECK(success_);

    switch (*button_index) {
      case BUTTON_COPY_TO_CLIPBOARD: {
        // To avoid keeping the screenshot image in memory, re-read the
        // screenshot file and copy it to the clipboard.
        base::ThreadPool::PostTask(
            FROM_HERE, kBlockingTaskTraits,
            base::BindOnce(&clipboard_util::ReadFileAndCopyToClipboardLocal,
                           screenshot_path_));
        base::RecordAction(base::UserMetricsAction("Screenshot_CopyClipboard"));
        break;
      }
      case BUTTON_ANNOTATE: {
        chromeos::NoteTakingHelper* helper = chromeos::NoteTakingHelper::Get();
        if (helper->IsAppAvailable(profile_)) {
          helper->LaunchAppForNewNote(profile_, screenshot_path_);
          base::RecordAction(base::UserMetricsAction("Screenshot_Annotate"));
        }
        break;
      }
      default:
        NOTREACHED() << "Unhandled button index " << *button_index;
    }
  }

 private:
  ~ScreenshotGrabberNotificationDelegate() override {}

  enum ButtonIndex {
    BUTTON_COPY_TO_CLIPBOARD = 0,
    BUTTON_ANNOTATE,
  };

  const bool success_;
  Profile* profile_;
  const base::FilePath screenshot_path_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotGrabberNotificationDelegate);
};

int GetScreenshotNotificationTitle(ScreenshotResult screenshot_result) {
  switch (screenshot_result) {
    case ScreenshotResult::DISABLED:
      return IDS_SCREENSHOT_NOTIFICATION_TITLE_DISABLED;
    case ScreenshotResult::DISABLED_BY_DLP:
      return IDS_SCREENSHOT_NOTIFICATION_TITLE_DISABLED_BY_DLP;
    case ScreenshotResult::SUCCESS:
      return IDS_SCREENSHOT_NOTIFICATION_TITLE_SUCCESS;
    default:
      return IDS_SCREENSHOT_NOTIFICATION_TITLE_FAIL;
  }
}

int GetScreenshotNotificationText(ScreenshotResult screenshot_result) {
  switch (screenshot_result) {
    case ScreenshotResult::DISABLED:
      return IDS_SCREENSHOT_NOTIFICATION_TEXT_DISABLED;
    case ScreenshotResult::DISABLED_BY_DLP:
      return IDS_SCREENSHOT_NOTIFICATION_TEXT_DISABLED_BY_DLP;
    case ScreenshotResult::SUCCESS:
      return IDS_SCREENSHOT_NOTIFICATION_TEXT_SUCCESS;
    default:
      return IDS_SCREENSHOT_NOTIFICATION_TEXT_FAIL;
  }
}

bool ShouldUse24HourClock() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (profile)
    return profile->GetPrefs()->GetBoolean(prefs::kUse24HourClock);
  return base::GetHourClockType() == base::k24HourClock;
}

bool GetScreenshotDirectory(base::FilePath* directory) {
  if (chromeos::LoginState::Get()->IsUserLoggedIn()) {
    DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
        ProfileManager::GetActiveUserProfile());
    *directory = download_prefs->DownloadPath();
  } else {
    if (!base::GetTempDir(directory)) {
      LOG(ERROR) << "Failed to find temporary directory.";
      return false;
    }
  }
  return true;
}

std::string GetScreenshotBaseFilename(const base::Time& unexploded_now) {
  base::Time::Exploded now;
  unexploded_now.LocalExplode(&now);

  // We don't use base/i18n/time_formatting.h here because it doesn't
  // support our format.  Don't use ICU either to avoid i18n file names
  // for non-English locales.
  // TODO(mukai): integrate this logic somewhere time_formatting.h
  std::string file_name = base::StringPrintf(
      "Screenshot %d-%02d-%02d at ", now.year, now.month, now.day_of_month);

  if (ShouldUse24HourClock()) {
    file_name.append(
        base::StringPrintf("%02d.%02d.%02d", now.hour, now.minute, now.second));
  } else {
    int hour = now.hour;
    if (hour > 12) {
      hour -= 12;
    } else if (hour == 0) {
      hour = 12;
    }
    file_name.append(
        base::StringPrintf("%d.%02d.%02d ", hour, now.minute, now.second));
    file_name.append((now.hour >= 12) ? "PM" : "AM");
  }

  return file_name;
}

// Read a file to a string and return.
std::string ReadFileToString(const base::FilePath& path) {
  std::string data;
  // It may fail, but it doesn't matter for our purpose.
  base::ReadFileToString(path, &data);
  return data;
}

using ShowNotificationCallback =
    base::OnceCallback<void(ScreenshotResult screenshot_result,
                            const base::FilePath& screenshot_path)>;

void SaveScreenshot(scoped_refptr<base::TaskRunner> ui_task_runner,
                    ShowNotificationCallback callback,
                    const base::FilePath& screenshot_path,
                    scoped_refptr<base::RefCountedMemory> png_data,
                    ScreenshotFileResult result,
                    const base::FilePath& local_path) {
  DCHECK(!base::CurrentUIThread::IsSet());
  DCHECK(!screenshot_path.empty());

  ScreenshotResult screenshot_result = ScreenshotResult::SUCCESS;
  switch (result) {
    case ScreenshotFileResult::SUCCESS:
      // Successfully got a local file to write to, write png data.
      DCHECK_GT(static_cast<int>(png_data->size()), 0);
      if (static_cast<size_t>(base::WriteFile(
              local_path, reinterpret_cast<const char*>(png_data->front()),
              static_cast<int>(png_data->size()))) != png_data->size()) {
        LOG(ERROR) << "Failed to save to " << local_path.value();
        screenshot_result = ScreenshotResult::WRITE_FILE_FAILED;
      }
      break;
    case ScreenshotFileResult::CHECK_DIR_FAILED:
      screenshot_result = ScreenshotResult::CHECK_DIR_FAILED;
      break;
    case ScreenshotFileResult::CREATE_DIR_FAILED:
      screenshot_result = ScreenshotResult::CREATE_DIR_FAILED;
      break;
    case ScreenshotFileResult::CREATE_FAILED:
      screenshot_result = ScreenshotResult::CREATE_FILE_FAILED;
      break;
  }

  // Report the result on the UI thread.
  ui_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), screenshot_result, screenshot_path));
}

void EnsureLocalDirectoryExists(
    const base::FilePath& path,
    ChromeScreenshotGrabber::FileCallback callback) {
  DCHECK(!base::CurrentUIThread::IsSet());
  DCHECK(!path.empty());

  if (!base::CreateDirectory(path.DirName())) {
    LOG(ERROR) << "Failed to ensure the existence of "
               << path.DirName().value();
    std::move(callback).Run(ScreenshotFileResult::CREATE_DIR_FAILED, path);
    return;
  }

  std::move(callback).Run(ScreenshotFileResult::SUCCESS, path);
}

}  // namespace

ChromeScreenshotGrabber::ChromeScreenshotGrabber()
    : screenshot_grabber_(new ui::ScreenshotGrabber) {
  DCHECK(!g_chrome_screenshot_grabber_instance);
  g_chrome_screenshot_grabber_instance = this;
}

ChromeScreenshotGrabber::~ChromeScreenshotGrabber() {
  DCHECK_EQ(this, g_chrome_screenshot_grabber_instance);
  g_chrome_screenshot_grabber_instance = nullptr;
}

// static
ChromeScreenshotGrabber* ChromeScreenshotGrabber::Get() {
  return g_chrome_screenshot_grabber_instance;
}

void ChromeScreenshotGrabber::HandleTakeScreenshotForAllRootWindows() {
  const ScreenshotArea area = ScreenshotArea::CreateForAllRootWindows();
  if (!CheckIfScreenshotAllowed(area)) {
    return;
  }

  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  // Reorder root_windows to take the primary root window's snapshot at first.
  aura::Window* primary_root = ash::Shell::GetPrimaryRootWindow();
  if (*(root_windows.begin()) != primary_root) {
    root_windows.erase(
        std::find(root_windows.begin(), root_windows.end(), primary_root));
    root_windows.insert(root_windows.begin(), primary_root);
  }
  base::Time time = base::Time::Now();
  for (size_t i = 0; i < root_windows.size(); ++i) {
    aura::Window* root_window = root_windows[i];
    gfx::Rect rect = root_window->bounds();

    base::Optional<int> display_id;
    if (root_windows.size() > 1)
      display_id = static_cast<int>(i + 1);
    screenshot_grabber_->TakeScreenshot(
        root_window, rect,
        base::BindOnce(&ChromeScreenshotGrabber::OnTookScreenshot,
                       weak_factory_.GetWeakPtr(), time, display_id, area));
  }
  base::RecordAction(base::UserMetricsAction("Screenshot_TakeFull"));
}

void ChromeScreenshotGrabber::HandleTakePartialScreenshot(
    aura::Window* window,
    const gfx::Rect& rect) {
  const ScreenshotArea area =
      ScreenshotArea::CreateForPartialWindow(window, rect);
  if (!CheckIfScreenshotAllowed(area)) {
    return;
  }

  screenshot_grabber_->TakeScreenshot(
      window, rect,
      base::BindOnce(&ChromeScreenshotGrabber::OnTookScreenshot,
                     weak_factory_.GetWeakPtr(), base::Time::Now(),
                     base::Optional<int>(), area));
  base::RecordAction(base::UserMetricsAction("Screenshot_TakePartial"));
}

void ChromeScreenshotGrabber::HandleTakeWindowScreenshot(aura::Window* window) {
  const ScreenshotArea area = ScreenshotArea::CreateForWindow(window);
  if (!CheckIfScreenshotAllowed(area)) {
    return;
  }

  screenshot_grabber_->TakeScreenshot(
      window, gfx::Rect(window->bounds().size()),
      base::BindOnce(&ChromeScreenshotGrabber::OnTookScreenshot,
                     weak_factory_.GetWeakPtr(), base::Time::Now(),
                     base::Optional<int>(), area));
  base::RecordAction(base::UserMetricsAction("Screenshot_TakeWindow"));
}

bool ChromeScreenshotGrabber::CanTakeScreenshot() {
  return screenshot_grabber_->CanTakeScreenshot();
}

void ChromeScreenshotGrabber::OnTookScreenshot(
    const base::Time& screenshot_time,
    const base::Optional<int>& display_num,
    const ScreenshotArea& area,
    ScreenshotResult result,
    scoped_refptr<base::RefCountedMemory> png_data) {
  if (result != ScreenshotResult::SUCCESS) {
    // We didn't complete taking the screenshot.
    OnScreenshotCompleted(result, base::FilePath());
    return;
  }

  if (!CheckIfScreenshotAllowed(area)) {
    return;
  }

  base::FilePath screenshot_directory;
  if (!GetScreenshotDirectory(&screenshot_directory)) {
    OnScreenshotCompleted(ScreenshotResult::GET_DIR_FAILED, base::FilePath());
    return;
  }

  // Calculate the path.
  std::string screenshot_basename = GetScreenshotBaseFilename(screenshot_time);
  if (display_num.has_value())
    screenshot_basename += base::StringPrintf(" - Display %d", *display_num);
  base::FilePath screenshot_path =
      screenshot_directory.AppendASCII(screenshot_basename + ".png");

  ShowNotificationCallback screenshot_complete_callback(
      base::BindOnce(&ChromeScreenshotGrabber::OnScreenshotCompleted,
                     weak_factory_.GetWeakPtr()));

  PrepareFileAndRunOnBlockingPool(
      screenshot_path,
      base::BindOnce(&SaveScreenshot, base::ThreadTaskRunnerHandle::Get(),
                     std::move(screenshot_complete_callback), screenshot_path,
                     png_data));
}

void ChromeScreenshotGrabber::PrepareFileAndRunOnBlockingPool(
    const base::FilePath& path,
    FileCallback callback) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(EnsureLocalDirectoryExists, path, std::move(callback)));
}

void ChromeScreenshotGrabber::OnScreenshotCompleted(
    ui::ScreenshotResult result,
    const base::FilePath& screenshot_path) {
  // Delegate to our tests in test mode.
  if (test_observer_)
    test_observer_->OnScreenshotCompleted(result, screenshot_path);

  // Do not show a notification that a screenshot was taken while no user is
  // logged in, since it is confusing for the user to get a message about it
  // after they log in (crbug.com/235217).
  if (!chromeos::LoginState::Get()->IsUserLoggedIn())
    return;

  if (result != ScreenshotResult::SUCCESS) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ChromeScreenshotGrabber::OnReadScreenshotFileForPreviewCompleted,
            weak_factory_.GetWeakPtr(), result, screenshot_path, gfx::Image()));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  SYSLOG(INFO) << "Screenshot taken";
#endif

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeScreenshotGrabber::ReadScreenshotFileForPreview,
                     weak_factory_.GetWeakPtr(), screenshot_path));
}

void ChromeScreenshotGrabber::ReadScreenshotFileForPreview(
    const base::FilePath& screenshot_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kBlockingTaskTraits,
      base::BindOnce(&ReadFileToString, screenshot_path),
      base::BindOnce(&ChromeScreenshotGrabber::DecodeScreenshotFileForPreview,
                     weak_factory_.GetWeakPtr(), screenshot_path));
}

void ChromeScreenshotGrabber::DecodeScreenshotFileForPreview(
    const base::FilePath& screenshot_path,
    std::string image_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (image_data.empty()) {
    LOG(ERROR) << "Failed to read the screenshot file: "
               << screenshot_path.value();
    OnReadScreenshotFileForPreviewCompleted(ScreenshotResult::SUCCESS,
                                            screenshot_path, gfx::Image());
    return;
  }

  // Decode the image in sandboxed process because decode image_data comes from
  // external storage.
  data_decoder::DecodeImageIsolated(
      std::vector<uint8_t>(image_data.begin(), image_data.end()),
      data_decoder::mojom::ImageCodec::DEFAULT, false,
      data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
      base::BindOnce(
          &ChromeScreenshotGrabber::OnScreenshotFileForPreviewDecoded,
          weak_factory_.GetWeakPtr(), screenshot_path));
}

void ChromeScreenshotGrabber::OnScreenshotFileForPreviewDecoded(
    const base::FilePath& screenshot_path,
    const SkBitmap& decoded_image) {
  OnReadScreenshotFileForPreviewCompleted(
      ScreenshotResult::SUCCESS, screenshot_path,
      gfx::Image::CreateFrom1xBitmap(decoded_image));
}

void ChromeScreenshotGrabber::OnReadScreenshotFileForPreviewCompleted(
    ScreenshotResult result,
    const base::FilePath& screenshot_path,
    gfx::Image image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const std::string notification_id(kNotificationId);
  // We cancel a previous screenshot notification, if any, to ensure we get
  // a fresh notification pop-up.
  NotificationDisplayService::GetForProfile(GetProfile())
      ->Close(NotificationHandler::Type::TRANSIENT, notification_id);

  const bool success = result == ScreenshotResult::SUCCESS;

  message_center::RichNotificationData optional_field;
  if (success) {
    // The order in which these buttons are added must be reflected by
    // ScreenshotGrabberNotificationDelegate::ButtonIndex.
    message_center::ButtonInfo copy_button(l10n_util::GetStringUTF16(
        IDS_SCREENSHOT_NOTIFICATION_BUTTON_COPY_TO_CLIPBOARD));
    optional_field.buttons.push_back(copy_button);

    if (chromeos::NoteTakingHelper::Get()->IsAppAvailable(GetProfile())) {
      message_center::ButtonInfo annotate_button(l10n_util::GetStringUTF16(
          IDS_SCREENSHOT_NOTIFICATION_BUTTON_ANNOTATE));
      optional_field.buttons.push_back(annotate_button);
    }

    // Assign image for notification preview. It might be empty.
    optional_field.image = image;
  }

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          image.IsEmpty() ? message_center::NOTIFICATION_TYPE_SIMPLE
                          : message_center::NOTIFICATION_TYPE_IMAGE,
          kNotificationId,
          l10n_util::GetStringUTF16(GetScreenshotNotificationTitle(result)),
          l10n_util::GetStringUTF16(GetScreenshotNotificationText(result)),
          /*display_source=*/base::string16() /*system name*/,
          GURL(kNotificationOriginUrl),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierScreenshot),
          optional_field,
          new ScreenshotGrabberNotificationDelegate(success, GetProfile(),
                                                    screenshot_path),
          vector_icons::kBusinessIcon,
          success ? message_center::SystemNotificationWarningLevel::NORMAL
                  : message_center::SystemNotificationWarningLevel::
                        CRITICAL_WARNING);

  NotificationDisplayService::GetForProfile(GetProfile())
      ->Display(NotificationHandler::Type::TRANSIENT, *notification,
                /*metadata=*/nullptr);

  if (success && ash::features::IsTemporaryHoldingSpaceEnabled()) {
    ash::HoldingSpaceKeyedServiceFactory::GetInstance()
        ->GetService(GetProfile())
        ->AddScreenshot(screenshot_path);
  }
}

Profile* ChromeScreenshotGrabber::GetProfile() {
  return ProfileManager::GetActiveUserProfile();
}

bool ChromeScreenshotGrabber::CheckIfScreenshotAllowed(
    const ScreenshotArea& area) {
  // Have three ways to disable screenshots:
  // - local state pref whose value is set from policy;
  // - simple flag which is set/unset when entering/exiting special modes where
  // screenshots should be disabled (pref is problematic because it's kept
  // across reboots, hence if the device crashes it may get stuck with the wrong
  // value).
  // - because of DataLeakPrevention restricted content present in the area of
  // the screenshot - handled in the second check.
  if (!screenshots_allowed_ || g_browser_process->local_state()->GetBoolean(
                                   prefs::kDisableScreenshots)) {
    OnScreenshotCompleted(ScreenshotResult::DISABLED, base::FilePath());
    return false;
  }
  if (policy::DlpContentManager::Get()->IsScreenshotRestricted(area)) {
    OnScreenshotCompleted(ScreenshotResult::DISABLED_BY_DLP, base::FilePath());
    return false;
  }
  return true;
}
