// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DEVICE_CHOOSER_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DEVICE_CHOOSER_CONTENT_VIEW_H_

#include <memory>

#include "base/strings/string16.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "ui/base/models/table_model.h"
#include "ui/gfx/range/range.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Checkbox;
class Label;
class LabelButton;
class TableView;
class TableViewObserver;
class Throbber;
}

// A bubble or dialog view for choosing among several options in a table.
// Used for WebUSB/WebBluetooth device selection for Chrome and extensions.
class DeviceChooserContentView : public views::View,
                                 public ui::TableModel,
                                 public ChooserController::View {
 public:
  METADATA_HEADER(DeviceChooserContentView);
  DeviceChooserContentView(
      views::TableViewObserver* table_view_observer,
      std::unique_ptr<ChooserController> chooser_controller);
  DeviceChooserContentView(const DeviceChooserContentView&) = delete;
  DeviceChooserContentView& operator=(const DeviceChooserContentView&) = delete;
  ~DeviceChooserContentView() override;

  // views::View:
  gfx::Size GetMinimumSize() const override;

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;
  gfx::ImageSkia GetIcon(int row) override;

  // ChooserController::View:
  void OnOptionsInitialized() override;
  void OnOptionAdded(size_t index) override;
  void OnOptionRemoved(size_t index) override;
  void OnOptionUpdated(size_t index) override;
  void OnAdapterEnabledChanged(bool enabled) override;
  void OnAdapterAuthorizationChanged(bool authorized) override;
  void OnRefreshStateChanged(bool refreshing) override;

  // Note that there is no way to update the window title - for any given
  // instance of DeviceChooserContentView, this method is only called once to
  // initially set the window title.
  base::string16 GetWindowTitle() const;
  std::unique_ptr<views::View> CreateExtraView();
  bool IsDialogButtonEnabled(ui::DialogButton button) const;
  void Accept();
  void Cancel();
  void Close();
  void UpdateTableView();
  void SelectAllCheckboxChanged();

  void ShowThrobber();
  void HideThrobber();
  void ShowReScanButton(bool enable);

  // Test-only accessors to children.
  views::TableView* table_view_for_testing() { return table_view_; }
  views::LabelButton* ReScanButtonForTesting();
  views::Throbber* ThrobberForTesting();
  views::Label* ThrobberLabelForTesting();

 private:
  friend class DeviceChooserContentViewTest;

  std::unique_ptr<ChooserController> chooser_controller_;

  // Boolean reflecting the status of the device adapter. For example if the
  // user has bluetooth turned on or off on their device. This is used to
  // ensure the users are always informed as to what is needed to get the
  // devices working.
  bool adapter_enabled_ = true;

  // Boolean reflecting the browsers authorization state. Currently only
  // Bluetooth on macOS requires applications to acquire permission. This
  // is used to ensure the users are always informed as to what is needed
  // to get the devices working in the browser.
  bool adapter_authorized_ = true;

  views::ScrollView* table_parent_ = nullptr;
  views::Checkbox* select_all_view_ = nullptr;
  views::TableView* table_view_ = nullptr;
  views::View* no_options_view_ = nullptr;
  views::View* adapter_off_view_ = nullptr;
  views::LabelButton* re_scan_button_ = nullptr;
  views::Throbber* throbber_ = nullptr;
  views::Label* throbber_label_ = nullptr;
  views::View* adapter_unauthorized_view_ = nullptr;

  bool is_initialized_ = false;
  base::CallbackListSubscription select_all_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DEVICE_CHOOSER_CONTENT_VIEW_H_
