// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCANNING_SCAN_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_SCANNING_SCAN_SERVICE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/components/scanning/mojom/scanning.mojom.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {

class LorgnetteScannerManager;

// Implementation of the chromeos::scanning::mojom::ScanService interface. Used
// by the scanning WebUI (chrome://scanning) to get connected scanners, obtain
// scanner capabilities, and perform scans.
class ScanService : public scanning::mojom::ScanService, public KeyedService {
 public:
  ScanService(LorgnetteScannerManager* lorgnette_scanner_manager,
              base::FilePath my_files_path,
              base::FilePath google_drive_path);
  ~ScanService() override;

  ScanService(const ScanService&) = delete;
  ScanService& operator=(const ScanService&) = delete;

  // scanning::mojom::ScanService:
  void GetScanners(GetScannersCallback callback) override;
  void GetScannerCapabilities(const base::UnguessableToken& scanner_id,
                              GetScannerCapabilitiesCallback callback) override;
  void StartScan(const base::UnguessableToken& scanner_id,
                 scanning::mojom::ScanSettingsPtr settings,
                 mojo::PendingRemote<scanning::mojom::ScanJobObserver> observer,
                 StartScanCallback callback) override;
  void CancelScan() override;

  // Binds receiver_ by consuming |pending_receiver|.
  void BindInterface(
      mojo::PendingReceiver<scanning::mojom::ScanService> pending_receiver);

  // Sets |google_drive_path_| for tests.
  void SetGoogleDrivePathForTesting(const base::FilePath& google_drive_path);

  // Sets |my_files_path_| for tests.
  void SetMyFilesPathForTesting(const base::FilePath& my_files_path);

 private:
  // KeyedService:
  void Shutdown() override;

  // Processes the result of calling LorgnetteScannerManager::GetScannerNames().
  void OnScannerNamesReceived(GetScannersCallback callback,
                              std::vector<std::string> scanner_names);

  // Processes the result of calling
  // LorgnetteScannerManager::GetScannerCapabilities().
  void OnScannerCapabilitiesReceived(
      GetScannerCapabilitiesCallback callback,
      const base::Optional<lorgnette::ScannerCapabilities>& capabilities);

  // Receives progress updates after calling LorgnetteScannerManager::Scan().
  // |page_number| indicates the page the |progress_percent| corresponds to.
  void OnProgressPercentReceived(uint32_t progress_percent,
                                 uint32_t page_number);

  // Processes each |scanned_image| received after calling
  // LorgnetteScannerManager::Scan(). |scan_to_path| is where images will be
  // saved, and |file_type| specifies the file type to use when saving scanned
  // images.
  void OnPageReceived(const base::FilePath& scan_to_path,
                      const scanning::mojom::FileType file_type,
                      std::string scanned_image,
                      uint32_t page_number);

  // Processes the final result of calling LorgnetteScannerManager::Scan().
  void OnScanCompleted(bool success);

  // Processes the final result of calling
  // LorgnetteScannerManager::CancelScan().
  void OnCancelCompleted(bool success);

  // Called once the task runner finishes saving a PDF file.
  void OnPdfSaved(const bool success);

  // Called once the task runner finishes saving a page of a scan.
  void OnPageSaved(const base::FilePath& saved_file_path);

  // Called once the task runner finishes saving the last page of a scan.
  void OnAllPagesSaved(bool success);

  // Sets the local member variables back to their initial empty state.
  void ClearScanState();

  // TODO(jschettler): Replace this with a generic helper function when one is
  // available.
  // Determines whether the service supports saving scanned images to
  // |file_path|.
  bool FilePathSupported(const base::FilePath& file_path);

  // Returns the scanner name corresponding to the given |scanner_id| or an
  // empty string if the name cannot be found.
  std::string GetScannerName(const base::UnguessableToken& scanner_id);

  // Map of scanner IDs to display names. Used to pass the correct display name
  // to LorgnetteScannerManager when clients provide an ID.
  base::flat_map<base::UnguessableToken, std::string> scanner_names_;

  // Receives and dispatches method calls to this implementation of the
  // chromeos::scanning::mojom::ScanService interface.
  mojo::Receiver<scanning::mojom::ScanService> receiver_{this};

  // Used to send scan job events to an observer. The remote is bound when a
  // scan job is started and is disconnected when the scan job is complete.
  mojo::Remote<scanning::mojom::ScanJobObserver> scan_job_observer_;

  // Unowned. Used to get scanner information and perform scans.
  LorgnetteScannerManager* lorgnette_scanner_manager_;

  // The paths to the user's My files and Google Drive directories. Used to
  // determine if a selected file path is supported.
  base::FilePath my_files_path_;
  base::FilePath google_drive_path_;

  // Indicates whether there was a failure to save scanned images.
  bool page_save_failed_;

  // The scanned images used to create a multipage PDF.
  std::vector<std::string> scanned_images_;

  // The time a scan was started. Used in filenames when saving scanned images.
  base::Time::Exploded start_time_;

  // The file path of the last page scanned in a scan job.
  base::FilePath last_scanned_file_path_;

  // Task runner used to convert and save scanned images.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Tracks the number of pages scanned for histogram recording.
  int num_pages_scanned_;

  // The time at which GetScanners() is called. Used to record the time between
  // a user launching the Scan app and being able to interact with it.
  base::TimeTicks get_scanners_time_;

  base::WeakPtrFactory<ScanService> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SCANNING_SCAN_SERVICE_H_
