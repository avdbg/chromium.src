// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_DETAILS_H_
#define CHROME_BROWSER_MEMORY_DETAILS_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/process_type.h"

namespace memory_instrumentation {
class GlobalMemoryDump;
}  // namespace memory_instrumentation

// We collect data about each browser process.  A browser may
// have multiple processes (of course!).  Even IE has multiple
// processes these days.
struct ProcessMemoryInformation {
  // NOTE: Do not remove or reorder the elements in this enum, and only add new
  // items at the end. We depend on these specific values in a histogram.
  enum RendererProcessType {
    RENDERER_UNKNOWN = 0,
    RENDERER_NORMAL,
    RENDERER_CHROME,        // WebUI (chrome:// URL)
    RENDERER_EXTENSION,     // chrome-extension://
    RENDERER_DEVTOOLS,      // Web inspector
    RENDERER_INTERSTITIAL,  // malware/phishing interstitial
    RENDERER_BACKGROUND_APP // hosted app background page
  };

  static std::string GetRendererTypeNameInEnglish(RendererProcessType type);
  static std::string GetFullTypeNameInEnglish(
      int process_type,
      RendererProcessType rtype);

  ProcessMemoryInformation();
  ProcessMemoryInformation(const ProcessMemoryInformation& other);
  ~ProcessMemoryInformation();

  // Default ordering is by private memory consumption.
  bool operator<(const ProcessMemoryInformation& rhs) const;

  // The process id.
  base::ProcessId pid;
  // The process version
  base::string16 version;
  // The process product name.
  base::string16 product_name;
  // The number of processes which this memory represents.
  int num_processes;
  // If this is a child process of Chrome, what type (i.e. plugin) it is.
  int process_type;
  // Number of open file descriptors in this process.
  int num_open_fds;
  // Maximum number of file descriptors that can be opened in this process.
  int open_fds_soft_limit;
  // If this is a renderer process, what type it is.
  RendererProcessType renderer_type;
  // A collection of titles used, i.e. for a tab it'll show all the page titles.
  std::vector<base::string16> titles;
  // Consistent memory metric for all platforms.
  size_t private_memory_footprint_kb;
};

typedef std::vector<ProcessMemoryInformation> ProcessMemoryInformationList;

// Browser Process Information.
struct ProcessData {
  ProcessData();
  ProcessData(const ProcessData& rhs);
  ~ProcessData();
  ProcessData& operator=(const ProcessData& rhs);

  base::string16 name;
  base::string16 process_name;
  ProcessMemoryInformationList processes;
};

// MemoryDetails fetches memory details about current running browsers.
// Because this data can only be fetched asynchronously, callers use
// this class via a callback.
//
// Example usage:
//
//    class MyMemoryDetailConsumer : public MemoryDetails {
//
//      MyMemoryDetailConsumer() {
//        // Anything but |StartFetch()|.
//      }
//
//      // (Or just call |StartFetch()| explicitly if there's nothing else to
//      // do.)
//      void StartDoingStuff() {
//        StartFetch();  // Starts fetching details.
//        // Etc.
//      }
//
//      // Your other class stuff here
//
//      virtual void OnDetailsAvailable() {
//        // do work with memory info here
//      }
//    }
class MemoryDetails : public base::RefCountedThreadSafe<MemoryDetails> {
 public:
  // Constructor.
  MemoryDetails();

  // Initiate updating the current memory details.  These are fetched
  // asynchronously because data must be collected from multiple threads.
  // OnDetailsAvailable will be called when this process is complete.
  void StartFetch();

  virtual void OnDetailsAvailable() = 0;

  // Returns a string summarizing memory usage of the Chrome browser process
  // and all sub-processes, suitable for logging.  Tab title may contain PII,
  // set |include_tab_title| to false to exclude tab titles when there are
  // privacy concerns.
  std::string ToLogString(bool include_tab_title);

 protected:
  friend class base::RefCountedThreadSafe<MemoryDetails>;

  virtual ~MemoryDetails();

  // Access to the process detail information.  This data is only available
  // after OnDetailsAvailable() has been called.
  const std::vector<ProcessData>& processes() { return process_data_; }

  // Returns a pointer to the ProcessData structure for Chrome.
  ProcessData* ChromeBrowser();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const base::SwapInfo& swap_info() const { return swap_info_; }
#endif

 private:
  // Collect child process information on the IO thread.  This is needed because
  // information about some child process types (i.e. plugins) can only be taken
  // on that thread.  The data will be used by about:memory.  When finished,
  // invokes back to the file thread to run the rest of the about:memory
  // functionality.
  void CollectChildInfoOnIOThread();

  // Collect current process information from the OS and store it
  // for processing.  If data has already been collected, clears old
  // data and re-collects the data.
  // Note - this function enumerates memory details from many processes
  // and is fairly expensive to run, hence it's run on the blocking pool.
  // The parameter holds information about processes from the IO thread.
  void CollectProcessData(
      const std::vector<ProcessMemoryInformation>& child_info);

  // Collect child process information on the UI thread.  Information about
  // renderer processes is only available there.
  void CollectChildInfoOnUIThread();

  void DidReceiveMemoryDump(
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

  std::vector<ProcessData> process_data_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::SwapInfo swap_info_;
#endif

  DISALLOW_COPY_AND_ASSIGN(MemoryDetails);
};

#endif  // CHROME_BROWSER_MEMORY_DETAILS_H_
