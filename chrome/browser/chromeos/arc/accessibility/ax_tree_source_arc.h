// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_AX_TREE_SOURCE_ARC_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_AX_TREE_SOURCE_ARC_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_info_data_wrapper.h"
#include "components/arc/mojom/accessibility_helper.mojom-forward.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/views/view.h"

namespace ui {
struct AXEvent;
}

namespace arc {
class AXTreeSourceArcTest;

using AXTreeArcSerializer = ui::AXTreeSerializer<AccessibilityInfoDataWrapper*>;

// This class represents the accessibility tree from the focused ARC window.
class AXTreeSourceArc : public ui::AXTreeSource<AccessibilityInfoDataWrapper*>,
                        public ui::AXActionHandler {
 public:
  class Delegate {
   public:
    virtual void OnAction(const ui::AXActionData& data) const = 0;
    virtual bool UseFullFocusMode() const = 0;
  };

  // The interface to hook the event handling and the node serialization.
  class Hook {
   public:
    Hook() = default;
    virtual ~Hook() = default;

    // Called prior to accessibility event dispatch.
    // Hook implementations can update the internal state if necessary so that
    // hooks can update the serialization state in PostSerializeNode().
    // Return true if re-serialization of attaching node is needed.
    virtual bool PreDispatchEvent(
        AXTreeSourceArc* tree_source,
        const mojom::AccessibilityEventData& event_data) = 0;

    // Called after the default serialization of the attaching node.
    // Hook implementations can modify the serialization of given |out_data|.
    // Note that serialization is executed only when ui::AXTreeSerializer calls
    // SerializeNode() from AXTreeSerializer.SerializeChanges().
    // To ensure the node re-serialized, the class must return |true| on
    // PreDispatchEvent() if the event is NOT coming from its ancestry.
    virtual void PostSerializeNode(ui::AXNodeData* out_data) const = 0;
  };

  explicit AXTreeSourceArc(Delegate* delegate);
  ~AXTreeSourceArc() override;

  // Notify automation of an accessibility event.
  void NotifyAccessibilityEvent(mojom::AccessibilityEventData* event_data);

  // Notify automation of a result to an action.
  void NotifyActionResult(const ui::AXActionData& data, bool result);

  // Notify automation of result to getTextLocation.
  void NotifyGetTextLocationDataResult(const ui::AXActionData& data,
                                       const base::Optional<gfx::Rect>& rect);

  // Invalidates the tree serializer.
  void InvalidateTree();

  // When it is enabled, this class exposes an accessibility tree optimized for
  // screen readers such as ChromeVox and SwitchAccess. This intends to have the
  // navigation order and focusabilities similar to TalkBack.
  // Also, when it is enabled, the accessibility focus in Android is exposed as
  // the focus of this tree.
  bool UseFullFocusMode() const;

  // Returns true if the node id is the root of the node tree (which can have a
  // parent window).
  // virtual for testing.
  virtual bool IsRootOfNodeTree(int32_t id) const;

  AccessibilityInfoDataWrapper* GetFirstImportantAncestor(
      AccessibilityInfoDataWrapper* info_data) const;

  // AXTreeSource:
  bool GetTreeData(ui::AXTreeData* data) const override;
  AccessibilityInfoDataWrapper* GetRoot() const override;
  AccessibilityInfoDataWrapper* GetFromId(int32_t id) const override;
  AccessibilityInfoDataWrapper* GetParent(
      AccessibilityInfoDataWrapper* info_data) const override;
  void SerializeNode(AccessibilityInfoDataWrapper* info_data,
                     ui::AXNodeData* out_data) const override;

  aura::Window* GetWindow() const;

  bool is_notification() { return is_notification_; }

  bool is_input_method_window() { return is_input_method_window_; }

  // The window id of this tree.
  base::Optional<int32_t> window_id() const { return window_id_; }

 private:
  friend class arc::AXTreeSourceArcTest;

  // Actual implementation of NotifyAccessibilityEvent.
  void NotifyAccessibilityEventInternal(
      const mojom::AccessibilityEventData& event_data);

  // virtual for testing.
  virtual extensions::AutomationEventRouterInterface* GetAutomationEventRouter()
      const;

  // Computes the smallest rect that encloses all of the descendants of
  // |info_data|.
  gfx::Rect ComputeEnclosingBounds(
      AccessibilityInfoDataWrapper* info_data) const;

  // Helper to recursively compute bounds for |info_data|. Returns true if
  // non-empty bounds were encountered.
  void ComputeEnclosingBoundsInternal(AccessibilityInfoDataWrapper* info_data,
                                      gfx::Rect* computed_bounds) const;

  // Find the most top-left focusable node under the given node in full focus mode.
  AccessibilityInfoDataWrapper* FindFirstFocusableNodeInFullFocusMode(
      AccessibilityInfoDataWrapper* info_data) const;

  // Updates android_focused_id_ from given AccessibilityEventData.
  // Having this method, |android_focused_id_| is one of these:
  // - input focus in Android
  // - accessibility focus in Android
  // - the chrome automation client's internal focus (via set sequential focus
  //   action and replying accessibility focus event from Android).
  // This returns false if we don't want to dispatch the processing
  // event to chrome automation. Otherwise, this returns true.
  bool UpdateAndroidFocusedId(const mojom::AccessibilityEventData& event_data);

  // Processes implementations of Hooks and returns a list node id that needs
  // re-serialization.
  std::vector<int32_t> ProcessHooksOnEvent(
      const mojom::AccessibilityEventData& event_data);

  // Compare previous live region and current live region, and add event to the
  // given vector if there is any difference.
  void HandleLiveRegions(std::vector<ui::AXEvent>* events);

  // Resets tree state.
  void Reset();

  // AXTreeSource:
  int32_t GetId(AccessibilityInfoDataWrapper* info_data) const override;
  void GetChildren(
      AccessibilityInfoDataWrapper* info_data,
      std::vector<AccessibilityInfoDataWrapper*>* out_children) const override;
  bool IsIgnored(AccessibilityInfoDataWrapper* info_data) const override;
  bool IsValid(AccessibilityInfoDataWrapper* info_data) const override;
  bool IsEqual(AccessibilityInfoDataWrapper* info_data1,
               AccessibilityInfoDataWrapper* info_data2) const override;
  AccessibilityInfoDataWrapper* GetNull() const override;

  // AXActionHandlerBase:
  void PerformAction(const ui::AXActionData& data) override;

  // Maps an AccessibilityInfoDataWrapper ID to its tree data.
  std::map<int32_t, std::unique_ptr<AccessibilityInfoDataWrapper>> tree_map_;

  // Maps an AccessibilityInfoDataWrapper ID to its parent.
  std::map<int32_t, int32_t> parent_map_;

  std::unique_ptr<AXTreeArcSerializer> current_tree_serializer_;
  base::Optional<int32_t> root_id_;
  base::Optional<int32_t> window_id_;
  base::Optional<int32_t> android_focused_id_;

  bool is_notification_;
  bool is_input_method_window_;

  base::Optional<std::string> notification_key_;

  // Cache of mapping from the *Android* window id to the last focused node id.
  std::map<int32_t, int32_t> window_id_to_last_focus_node_id_;

  // Mapping from Chrome node ID to its cached computed bounds.
  // This simplifies bounds calculations.
  std::map<int32_t, gfx::Rect> computed_bounds_;

  // Mapping from Chrome node ID to the previous computed name for live region.
  std::map<int32_t, std::string> previous_live_region_name_;

  // Mapping from Chrome node ID to the attached hook implementations.
  base::flat_map<int32_t, std::unique_ptr<Hook>> hooks_;

  // A delegate that handles accessibility actions on behalf of this tree. The
  // delegate is valid during the lifetime of this tree.
  const Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceArc);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_AX_TREE_SOURCE_ARC_H_
