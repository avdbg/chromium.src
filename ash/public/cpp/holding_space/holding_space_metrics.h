// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {
namespace holding_space_metrics {

// Enumeration of actions that can be taken on the holding space pod in the
// shelf. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class PodAction {
  // kClick (Deprecated) = 0,
  kShowBubble = 1,
  kCloseBubble = 2,
  kShowContextMenu = 3,
  kShowPreviews = 4,
  kHidePreviews = 5,
  kShowPod = 6,
  kHidePod = 7,
  kDragAndDropToPin = 8,
  kMaxValue = kDragAndDropToPin,
};

// Records the specified `action` taken on the holding space pod in the shelf.
ASH_PUBLIC_EXPORT void RecordPodAction(PodAction action);

// Enumeration of actions that can be taken on the holding space downloads
// button. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class DownloadsAction {
  kClick = 0,
  kMaxValue = kClick,
};

// Records the specified `action` taken on the holding space downloads header.
ASH_PUBLIC_EXPORT void RecordDownloadsAction(DownloadsAction action);

// Enumeration of actions that can be taken on the holding space Files app chip.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FilesAppChipAction {
  kClick = 0,
  kMaxValue = kClick,
};

// Records the specified `action` taken on the holding space Files app chip.
ASH_PUBLIC_EXPORT void RecordFilesAppChipAction(FilesAppChipAction action);

// Enumeration of actions that can be taken on holding space items. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class ItemAction {
  kCopy = 0,
  kDrag = 1,
  kLaunch = 2,
  kPin = 3,
  kShowInFolder = 4,
  kUnpin = 5,
  kRemove = 6,
  kMaxValue = kRemove,
};

// Records the specified `action` taken on a set of holding space `items`.
ASH_PUBLIC_EXPORT void RecordItemAction(
    const std::vector<const HoldingSpaceItem*>& items,
    ItemAction action);

// Records counts for the specified holding space `items`.
ASH_PUBLIC_EXPORT void RecordItemCounts(
    const std::vector<const HoldingSpaceItem*>& items);

// Records a failure to launch a holding space item of the specified `type`.
ASH_PUBLIC_EXPORT void RecordItemFailureToLaunch(HoldingSpaceItem::Type type);

// Records time from the first availability of the holding space feature to the
// first item being added to holding space.
ASH_PUBLIC_EXPORT void RecordTimeFromFirstAvailabilityToFirstAdd(
    base::TimeDelta time_delta);

// Records time from first availability to the first entry into holding space.
ASH_PUBLIC_EXPORT void RecordTimeFromFirstAvailabilityToFirstEntry(
    base::TimeDelta time_delta);

// Records time from first entry to the first pin into holding space.
ASH_PUBLIC_EXPORT void RecordTimeFromFirstEntryToFirstPin(
    base::TimeDelta time_delta);

// Records the `smoothness` of the holding space bubble resize animation. Note
// that `smoothness` is expected to be between 0 and 100 (inclusively) with
// 100 representing ideal smoothness of >= 60 frames per second.
ASH_PUBLIC_EXPORT void RecordBubbleResizeAnimationSmoothness(int smoothness);

// Records the `smoothness` of the holding space pod resize animation. Note that
// `smoothness` is expected to be between 0 and 100 (inclusively) with 100
// representing ideal smoothness of >= 60 frames per second.
ASH_PUBLIC_EXPORT void RecordPodResizeAnimationSmoothness(int smoothness);

}  // namespace holding_space_metrics
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_
