// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCANNING_LORGNETTE_SCANNER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_SCANNING_LORGNETTE_SCANNER_MANAGER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/dbus/lorgnette_manager/lorgnette_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {

class ZeroconfScannerDetector;

// Top-level manager of available scanners in Chrome OS. All functions in this
// class must be called from a sequenced context.
class LorgnetteScannerManager : public KeyedService {
 public:
  using GetScannerNamesCallback =
      base::OnceCallback<void(std::vector<std::string> scanner_names)>;
  using GetScannerCapabilitiesCallback = base::OnceCallback<void(
      const base::Optional<lorgnette::ScannerCapabilities>& capabilities)>;
  using ProgressCallback =
      base::RepeatingCallback<void(uint32_t progress_percent,
                                   uint32_t page_number)>;
  using PageCallback = base::RepeatingCallback<void(std::string scan_data,
                                                    uint32_t page_number)>;
  using CompletionCallback = base::OnceCallback<void(bool success)>;
  using CancelCallback = base::OnceCallback<void(bool success)>;

  ~LorgnetteScannerManager() override = default;

  static std::unique_ptr<LorgnetteScannerManager> Create(
      std::unique_ptr<ZeroconfScannerDetector> zeroconf_scanner_detector);

  // Returns the names of all available, deduplicated scanners.
  virtual void GetScannerNames(GetScannerNamesCallback callback) = 0;

  // Returns the capabilities of the scanner specified by |scanner_name|. If
  // |scanner_name| does not correspond to a known scanner, base::nullopt is
  // returned in the callback.
  virtual void GetScannerCapabilities(
      const std::string& scanner_name,
      GetScannerCapabilitiesCallback callback) = 0;

  // Performs a scan with the scanner specified by |scanner_name| using the
  // given |scan_properties|. As each page is scanned, |progress_callback| is
  // called with the current progress percent from 0 to 100. As each scanned
  // page is completed, |page_callback| is called with the image data for that
  // page. If |scanner_name| does not correspond to a known scanner, false is
  // returned in |completion_callback|. After the scan has completed,
  // |completion_callback| will be called with argument success=true.
  virtual void Scan(const std::string& scanner_name,
                    const lorgnette::ScanSettings& settings,
                    ProgressCallback progress_callback,
                    PageCallback page_callback,
                    CompletionCallback completion_callback) = 0;

  // Request to cancel the currently running scan job. This function makes the
  // assumption that LorgnetteManagerClient only has one scan running at a time.
  virtual void CancelScan(CancelCallback cancel_callback) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SCANNING_LORGNETTE_SCANNER_MANAGER_H_
