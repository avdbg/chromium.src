// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_

#include <memory>
#include <queue>
#include <set>

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/text_element_timing.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class WebString;
class LayoutBoxModelObject;
class LocalFrameView;
class PropertyTreeStateOrAlias;
class TextElementTiming;
class TracedValue;

class TextRecord : public base::SupportsWeakPtr<TextRecord> {
 public:
  TextRecord(DOMNodeId new_node_id,
             uint64_t new_first_size,
             const FloatRect& element_timing_rect,
             const IntRect& frame_visual_rect,
             const FloatRect& root_visual_rect)
      : node_id(new_node_id),
        first_size(new_first_size),
        element_timing_rect_(element_timing_rect) {
    static unsigned next_insertion_index_ = 1;
    insertion_index_ = next_insertion_index_++;
    if (PaintTimingVisualizer::IsTracingEnabled()) {
      lcp_rect_info_ = std::make_unique<LCPRectInfo>(
          frame_visual_rect, RoundedIntRect(root_visual_rect));
    }
  }
  TextRecord(const TextRecord&) = delete;
  TextRecord& operator=(const TextRecord&) = delete;

  DOMNodeId node_id = kInvalidDOMNodeId;
  uint64_t first_size = 0;
  // |insertion_index_| is ordered by insertion time, used as a secondary key
  // for ranking.
  unsigned insertion_index_ = 0;
  FloatRect element_timing_rect_;
  std::unique_ptr<LCPRectInfo> lcp_rect_info_;
  // The time of the first paint after fully loaded.
  base::TimeTicks paint_time = base::TimeTicks();
};

class CORE_EXPORT LargestTextPaintManager final
    : public GarbageCollected<LargestTextPaintManager> {
  using TextRecordSetComparator = bool (*)(const base::WeakPtr<TextRecord>&,
                                           const base::WeakPtr<TextRecord>&);
  using TextRecordSet =
      std::set<base::WeakPtr<TextRecord>, TextRecordSetComparator>;

 public:
  LargestTextPaintManager(LocalFrameView*, PaintTimingDetector*);
  LargestTextPaintManager(const LargestTextPaintManager&) = delete;
  LargestTextPaintManager& operator=(const LargestTextPaintManager&) = delete;

  inline void RemoveVisibleRecord(base::WeakPtr<TextRecord> record) {
    DCHECK(record);
    size_ordered_set_.erase(record);
    if (cached_largest_paint_candidate_.get() == record.get())
      cached_largest_paint_candidate_ = nullptr;
    is_result_invalidated_ = true;
  }

  base::WeakPtr<TextRecord> FindLargestPaintCandidate();

  void ReportCandidateToTrace(const TextRecord&);
  void ReportNoCandidateToTrace();
  base::WeakPtr<TextRecord> UpdateCandidate();
  void PopulateTraceValue(TracedValue&, const TextRecord& first_text_paint);
  inline void SetCachedResultInvalidated(bool value) {
    is_result_invalidated_ = value;
  }

  inline void InsertRecord(base::WeakPtr<TextRecord> record) {
    size_ordered_set_.emplace(record);
    SetCachedResultInvalidated(true);
  }

  void MaybeUpdateLargestIgnoredText(const LayoutObject&,
                                     const uint64_t&,
                                     const IntRect& frame_visual_rect,
                                     const FloatRect& root_visual_rect);
  std::unique_ptr<TextRecord> PopLargestIgnoredText() {
    return std::move(largest_ignored_text_);
  }

  void Trace(Visitor*) const;

 private:
  friend class LargestContentfulPaintCalculatorTest;
  friend class TextPaintTimingDetectorTest;

  TextRecordSet size_ordered_set_;
  base::WeakPtr<TextRecord> cached_largest_paint_candidate_;
  // This is used to cache the largest text paint result for better
  // efficiency.
  // The result will be invalidated whenever any change is done to the
  // variables used in |FindLargestPaintCandidate|.
  bool is_result_invalidated_ = false;
  unsigned count_candidates_ = 0;

  // Text paints are ignored when they (or an ancestor) have opacity 0. This can
  // be a problem later on if the opacity changes to nonzero but this change is
  // composited. We solve this for the special case of documentElement by
  // storing a record for the largest ignored text without nested opacity. We
  // consider this an LCP candidate when the documentElement's opacity changes
  // from zero to nonzero.
  std::unique_ptr<TextRecord> largest_ignored_text_;

  Member<const LocalFrameView> frame_view_;
  Member<PaintTimingDetector> paint_timing_detector_;
};

class CORE_EXPORT TextRecordsManager {
  DISALLOW_NEW();
  friend class TextPaintTimingDetectorTest;

 public:
  TextRecordsManager(LocalFrameView*, PaintTimingDetector*);
  TextRecordsManager(const TextRecordsManager&) = delete;
  TextRecordsManager& operator=(const TextRecordsManager&) = delete;

  void RemoveVisibleRecord(const LayoutObject&);
  void RemoveInvisibleRecord(const LayoutObject&);
  void RecordVisibleObject(const LayoutObject&,
                           const uint64_t& visual_size,
                           const FloatRect& element_timing_rect,
                           const IntRect& frame_visual_rect,
                           const FloatRect& root_visual_rect);
  void RecordInvisibleObject(const LayoutObject& object);
  bool NeedMeausuringPaintTime() const {
    return !texts_queued_for_paint_time_.IsEmpty() ||
           !size_zero_texts_queued_for_paint_time_.IsEmpty();
  }
  void AssignPaintTimeToQueuedRecords(const base::TimeTicks&);

  inline bool HasRecorded(const LayoutObject& object) const {
    return visible_objects_.Contains(&object) ||
           invisible_objects_.Contains(&object);
  }

  inline bool IsKnownVisible(const LayoutObject& object) const {
    return visible_objects_.Contains(&object);
  }

  inline bool IsKnownInvisible(const LayoutObject& object) const {
    return invisible_objects_.Contains(&object);
  }

  void CleanUpLargestTextPaint();

  bool HasTextElementTiming() const { return text_element_timing_; }
  void SetTextElementTiming(TextElementTiming* text_element_timing) {
    text_element_timing_ = text_element_timing;
  }

  inline base::WeakPtr<TextRecord> UpdateCandidate() {
    DCHECK(ltp_manager_);
    return ltp_manager_->UpdateCandidate();
  }

  // Receives a candidate text painted under opacity 0 but without nested
  // opacity. May update |largest_ignored_text_| if the new candidate has a
  // larger size.
  void MaybeUpdateLargestIgnoredText(const LayoutObject& object,
                                     const uint64_t& size,
                                     const IntRect& aggregated_visual_rect,
                                     const FloatRect& mapped_visual_rect) {
    DCHECK(ltp_manager_);
    ltp_manager_->MaybeUpdateLargestIgnoredText(
        object, size, aggregated_visual_rect, mapped_visual_rect);
  }
  // Called when documentElement changes from zero to nonzero opacity. Makes the
  // largest text that was hidden due to this a Largest Contentful Paint
  // candidate.
  void ReportLargestIgnoredText();

  inline bool IsRecordingLargestTextPaint() const { return ltp_manager_; }

  void Trace(Visitor*) const;

 private:
  friend class LargestContentfulPaintCalculatorTest;
  friend class TextPaintTimingDetectorTest;
  inline void QueueToMeasurePaintTime(base::WeakPtr<TextRecord>& record) {
    texts_queued_for_paint_time_.push_back(std::move(record));
  }

  // Once LayoutObject* is destroyed, |visible_objects_| and
  // |invisible_objects_| must immediately clear the corresponding record from
  // themselves.
  HashMap<const LayoutObject*, std::unique_ptr<TextRecord>> visible_objects_;
  HashSet<const LayoutObject*> invisible_objects_;

  Deque<base::WeakPtr<TextRecord>> texts_queued_for_paint_time_;
  // These are text records created to notify Element Timing of texts which are
  // first painted outside of the viewport. These have size 0 for the purpose of
  // LCP computations, even if the size of the text itself is not 0. They are
  // considered invisible objects by Largest Contentful Paint.
  Deque<std::unique_ptr<TextRecord>> size_zero_texts_queued_for_paint_time_;
  Member<LargestTextPaintManager> ltp_manager_;
  Member<TextElementTiming> text_element_timing_;
};

// TextPaintTimingDetector contains Largest Text Paint and support for Text
// Element Timing.
//
// Largest Text Paint timing measures when the largest text element gets painted
// within viewport. Specifically, it:
// 1. Tracks all texts' first paints, recording their visual size, paint time.
// 2. Every 1 second after the first text pre-paint, the algorithm starts an
// analysis. In the analysis:
// 2.1 Largest Text Paint finds the text with the  largest first visual size,
// reports its first paint time as a candidate result.
//
// For all these candidate results, Telemetry picks the lastly reported
// Largest Text Paint candidate as the final result.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit#heading=h.lvno2v283uls
class CORE_EXPORT TextPaintTimingDetector final
    : public GarbageCollected<TextPaintTimingDetector> {
  friend class TextPaintTimingDetectorTest;

 public:
  explicit TextPaintTimingDetector(LocalFrameView*,
                                   PaintTimingDetector*,
                                   PaintTimingCallbackManager*);
  TextPaintTimingDetector(const TextPaintTimingDetector&) = delete;
  TextPaintTimingDetector& operator=(const TextPaintTimingDetector&) = delete;

  bool ShouldWalkObject(const LayoutBoxModelObject&) const;
  void RecordAggregatedText(const LayoutBoxModelObject& aggregator,
                            const IntRect& aggregated_visual_rect,
                            const PropertyTreeStateOrAlias&);
  void OnPaintFinished();
  void LayoutObjectWillBeDestroyed(const LayoutObject&);
  void StopRecordingLargestTextPaint();
  void ResetCallbackManager(PaintTimingCallbackManager* manager) {
    callback_manager_ = manager;
  }
  inline bool IsRecordingLargestTextPaint() const {
    return records_manager_.IsRecordingLargestTextPaint();
  }
  inline base::WeakPtr<TextRecord> UpdateCandidate() {
    return records_manager_.UpdateCandidate();
  }
  void ReportLargestIgnoredText();
  void ReportPresentationTime(base::TimeTicks timestamp);
  void Trace(Visitor*) const;

 private:
  friend class LargestContentfulPaintCalculatorTest;

  void RegisterNotifyPresentationTime(
      PaintTimingCallbackManager::LocalThreadCallback callback);

  TextRecordsManager records_manager_;

  Member<PaintTimingCallbackManager> callback_manager_;

  // Make sure that at most one presentation promise is ongoing.
  bool awaiting_presentation_promise_ = false;

  bool need_update_timing_at_frame_end_ = false;

  Member<const LocalFrameView> frame_view_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_
