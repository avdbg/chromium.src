// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_list_observer.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class DesktopMediaPickerDialogView;

// This class is the controller for a View that displays a DesktopMediaList. It
// is responsible for:
//   * Observing a DesktopMediaList
//   * Updating its controlled view when that DesktopMediaList changes
//   * Providing access to the state of its controlled view to the dialog
//   * Proxying between its controlled view's callbacks and the dialog's
//     callbacks
class DesktopMediaListController : public DesktopMediaListObserver,
                                   public views::ViewObserver {
 public:
  // The interface implemented by a controlled view or one of its helper classes
  // to listen for updates to the source list.
  class SourceListListener {
   public:
    virtual void OnSourceAdded(size_t index) = 0;
    virtual void OnSourceRemoved(size_t index) = 0;
    virtual void OnSourceMoved(size_t old_index, size_t new_index) = 0;
    virtual void OnSourceNameChanged(size_t index) = 0;
    virtual void OnSourceThumbnailChanged(size_t index) = 0;
  };

  // The abstract interface implemented by any view controlled by this
  // controller.
  class ListView : public views::View {
   public:
    METADATA_HEADER(ListView);

    // Returns the DesktopMediaID of the selected element of this list, or
    // nullopt if no element is selected.
    virtual base::Optional<content::DesktopMediaID> GetSelection() = 0;

    // Returns the SourceListListener to use to notify this ListView of changes
    // to the backing DesktopMediaList.
    virtual SourceListListener* GetSourceListListener() = 0;

   protected:
    ListView() = default;
    ~ListView() override = default;
  };

  DesktopMediaListController(DesktopMediaPickerDialogView* dialog,
                             std::unique_ptr<DesktopMediaList> media_list);
  ~DesktopMediaListController() override;

  // Create this controller's corresponding View. There can only be one View per
  // controller; attempting to call this method twice on the same
  // DesktopMediaListController is not allowed.
  std::unique_ptr<views::View> CreateView(
      DesktopMediaSourceViewStyle generic_style,
      DesktopMediaSourceViewStyle single_style,
      const base::string16& accessible_name);

  std::unique_ptr<views::View> CreateTabListView(
      const base::string16& accessible_name);

  // Starts observing the DesktopMediaList given earlier, ignoring any entries
  // whose id matches dialog_window_id.
  void StartUpdating(content::DesktopMediaID dialog_window_id);

  // Focuses this controller's view.
  void FocusView();

  // Returns the DesktopMediaID corresponding to the current selection in this
  // controller's view, if there is one.
  base::Optional<content::DesktopMediaID> GetSelection() const;

  // These three methods are called by the view to inform the controller of
  // events. The first two indicate changes in the visual state of the view; the
  // last indicates that the user performed an action on the source view that
  // should serve to accept the entire dialog.
  void OnSourceListLayoutChanged();
  void OnSourceSelectionChanged();
  void AcceptSource();

  // These methods are used by the view (or its subviews) to query and
  // update the underlying DesktopMediaList.
  size_t GetSourceCount() const;
  const DesktopMediaList::Source& GetSource(size_t index) const;
  void SetThumbnailSize(const gfx::Size& size);

 private:
  friend class DesktopMediaPickerViewsTestApi;

  // This method is used as a callback to support source auto-selection, which
  // is used in some tests; it acts as though the user had selected (in the
  // view) the element corresponding to the given source and accepted the
  // dialog.
  void AcceptSpecificSource(content::DesktopMediaID source);

  // Analogous to AcceptSpecificSource, but rejects rather than accepts.
  // Used in tests.
  void Reject();

  // DesktopMediaListObserver:
  void OnSourceAdded(DesktopMediaList* list, int index) override;
  void OnSourceRemoved(DesktopMediaList* list, int index) override;
  void OnSourceMoved(DesktopMediaList* list,
                     int old_index,
                     int new_index) override;
  void OnSourceNameChanged(DesktopMediaList* list, int index) override;
  void OnSourceThumbnailChanged(DesktopMediaList* list, int index) override;

  // ViewObserver:
  void OnViewIsDeleting(views::View* view) override;

  bool ShouldAutoAccept(const DesktopMediaList::Source& source) const;
  bool ShouldAutoReject(const DesktopMediaList::Source& source) const;

  DesktopMediaPickerDialogView* dialog_;
  std::unique_ptr<DesktopMediaList> media_list_;
  ListView* view_ = nullptr;
  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      view_observations_{this};

  // Auto-selection. Used only in tests.
  const std::string auto_select_source_;
  const bool auto_accept_tab_capture_;
  const bool auto_reject_tab_capture_;

  base::WeakPtrFactory<DesktopMediaListController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_CONTROLLER_H_
