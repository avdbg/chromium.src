// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_SEARCH_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_SEARCH_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "chromeos/components/local_search_service/index.h"
#include "chromeos/components/local_search_service/shared_structs.h"

namespace chromeos {
namespace local_search_service {

class InvertedIndex;

// An implementation of Index.
// A search via the inverted index backend with TF-IDF based document ranking.
class InvertedIndexSearch : public Index {
 public:
  explicit InvertedIndexSearch(IndexId index_id);
  ~InvertedIndexSearch() override;

  InvertedIndexSearch(const InvertedIndexSearch&) = delete;
  InvertedIndexSearch& operator=(const InvertedIndexSearch&) = delete;

  // Index overrides:
  // GetSize is only accurate if the index has done updating.
  void GetSize(GetSizeCallback callback) override;
  void AddOrUpdate(const std::vector<Data>& data,
                   AddOrUpdateCallback callback) override;
  void Delete(const std::vector<std::string>& ids,
              DeleteCallback callback) override;
  void UpdateDocuments(const std::vector<Data>& data,
                       UpdateDocumentsCallback callback) override;
  void Find(const base::string16& query,
            uint32_t max_results,
            FindCallback callback) override;
  void ClearIndex(ClearIndexCallback callback) override;

  // Returns document id and number of occurrences of |term|.
  // Document ids are sorted in alphabetical order.
  std::vector<std::pair<std::string, uint32_t>> FindTermForTesting(
      const base::string16& term) const;

 private:
  void FinalizeAddOrUpdate(
      AddOrUpdateCallback callback,
      const std::vector<std::pair<std::string, std::vector<Token>>>& documents);

  // FinalizeDelete is called if Delete cannot be immediately done because
  // there's another index updating operation before it, i.e.
  // |num_queued_index_updates_| is not zero.
  void FinalizeDelete(DeleteCallback callback,
                      const std::vector<std::string>& ids);

  void FinalizeUpdateDocuments(
      UpdateDocumentsCallback callback,
      const std::vector<std::pair<std::string, std::vector<Token>>>& documents);

  // In order to reduce unnecessary inverted index building, we only build the
  // index if there's no upcoming modification to the index's document list.
  void MaybeBuildInvertedIndex();

  // AddOrUpdate requires content extraction to be done before index is updated
  // (tokens added, index built). As content extraction runs on another thread
  // (|blocking_task_runner_|), we need to keep track of how many index-update
  // operations are to be done (and queued). Delete may be queued as well if
  // there is an AddOrUpdate before it. We need to ensure documents are added or
  // modified or deleted in the same order as they're given by the index client.
  int num_queued_index_updates_ = 0;

  std::unique_ptr<InvertedIndex> inverted_index_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InvertedIndexSearch> weak_ptr_factory_{this};
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_SEARCH_H_
