// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/android/reading_list_manager_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/reading_list/android/reading_list_manager.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using BookmarkNode = bookmarks::BookmarkNode;
using ReadingListEntries = ReadingListModelImpl::ReadingListEntries;

namespace {

constexpr char kURL[] = "https://www.example.com";
constexpr char kURL1[] = "https://www.anotherexample.com";
constexpr char kTitle[] =
    "In earlier tellings, the dog had a better reputation than the cat, "
    "however the president vetoed it.";
constexpr char kTitle1[] = "boring title about dogs.";
constexpr char kReadStatusKey[] = "read_status";
constexpr char kReadStatusRead[] = "true";
constexpr char kReadStatusUnread[] = "false";
constexpr char kInvalidUTF8[] = "\xc3\x28";

class MockObserver : public ReadingListManager::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  // ReadingListManager::Observer implementation.
  MOCK_METHOD(void, ReadingListLoaded, (), (override));
  MOCK_METHOD(void, ReadingListChanged, (), (override));
};

class ReadingListManagerImplTest : public testing::Test {
 public:
  ReadingListManagerImplTest() = default;
  ~ReadingListManagerImplTest() override = default;

  void SetUp() override {
    reading_list_model_ = std::make_unique<ReadingListModelImpl>(
        /*storage_layer=*/nullptr, /*pref_service=*/nullptr, &clock_);
    manager_ =
        std::make_unique<ReadingListManagerImpl>(reading_list_model_.get());
    manager_->AddObserver(observer());
    EXPECT_TRUE(manager()->IsLoaded());
  }

  void TearDown() override { manager_->RemoveObserver(observer()); }

 protected:
  ReadingListManager* manager() { return manager_.get(); }
  ReadingListModelImpl* reading_list_model() {
    return reading_list_model_.get();
  }
  base::SimpleTestClock* clock() { return &clock_; }
  MockObserver* observer() { return &observer_; }

  const BookmarkNode* Add(const GURL& url, const std::string& title) {
    EXPECT_CALL(*observer(), ReadingListChanged());
    return manager()->Add(url, title);
  }

  void Delete(const GURL& url) {
    EXPECT_CALL(*observer(), ReadingListChanged());
    manager()->Delete(url);
  }

  void SetReadStatus(const GURL& url, bool read) {
    EXPECT_CALL(*observer(), ReadingListChanged());
    manager()->SetReadStatus(url, read);
  }

 private:
  base::SimpleTestClock clock_;
  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  std::unique_ptr<ReadingListManager> manager_;
  MockObserver observer_;
};

// Verifies the states without any reading list data.
TEST_F(ReadingListManagerImplTest, RootWithEmptyReadingList) {
  const auto* root = manager()->GetRoot();
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_folder());
  EXPECT_EQ(0u, manager()->size());
}

// Verifies load data into reading list model will update |manager_| as well.
TEST_F(ReadingListManagerImplTest, Load) {
  // Load data into reading list model.
  auto entries = std::make_unique<ReadingListEntries>();
  GURL url(kURL);
  entries->emplace(url, ReadingListEntry(url, kTitle, clock()->Now()));
  reading_list_model()->StoreLoaded(std::move(entries));

  const auto* node = manager()->Get(url);
  EXPECT_TRUE(node);
  EXPECT_EQ(url, node->url());
  EXPECT_EQ(1u, manager()->size());
  EXPECT_EQ(1u, manager()->unread_size());
}

// Verifes Add(), Get(), Delete() API in reading list manager.
TEST_F(ReadingListManagerImplTest, AddGetDelete) {
  // Adds a node.
  GURL url(kURL);
  Add(url, kTitle);
  EXPECT_EQ(1u, manager()->size());
  EXPECT_EQ(1u, manager()->unread_size());
  EXPECT_EQ(1u, manager()->GetRoot()->children().size())
      << "The reading list node should be the child of the root.";

  // Gets the node, and verifies its content.
  const BookmarkNode* node = manager()->Get(url);
  ASSERT_TRUE(node);
  EXPECT_EQ(url, node->url());
  EXPECT_EQ(kTitle, base::UTF16ToUTF8(node->GetTitle()));
  std::string read_status;
  node->GetMetaInfo(kReadStatusKey, &read_status);
  EXPECT_EQ(kReadStatusUnread, read_status)
      << "By default the reading list node is marked as unread.";

  // Gets an invalid URL.
  EXPECT_EQ(nullptr, manager()->Get(GURL("invalid spec")));

  // Deletes the node.
  Delete(url);
  EXPECT_EQ(0u, manager()->size());
  EXPECT_EQ(0u, manager()->unread_size());
  EXPECT_TRUE(manager()->GetRoot()->children().empty());
}

// Verifies GetNodeByID() and IsReadingListBookmark() works correctly.
TEST_F(ReadingListManagerImplTest, GetNodeByIDIsReadingListBookmark) {
  GURL url(kURL);
  const auto* node = Add(url, kTitle);

  // Find the root.
  EXPECT_EQ(manager()->GetRoot(),
            manager()->GetNodeByID(manager()->GetRoot()->id()));
  EXPECT_TRUE(manager()->IsReadingListBookmark(manager()->GetRoot()));

  // Find existing node.
  EXPECT_EQ(node, manager()->GetNodeByID(node->id()));
  EXPECT_TRUE(manager()->IsReadingListBookmark(node));

  // Non existing node.
  node = manager()->GetNodeByID(12345);
  EXPECT_FALSE(node);
  EXPECT_FALSE(manager()->IsReadingListBookmark(node));

  // Node with the same URL but not in the tree.
  auto node_same_url =
      std::make_unique<BookmarkNode>(0, base::GUID::GenerateRandomV4(), url);
  EXPECT_FALSE(manager()->IsReadingListBookmark(node_same_url.get()));
}

// Verifies GetMatchingNodes() API in reading list manager.
TEST_F(ReadingListManagerImplTest, GetMatchingNodes) {
  manager()->Add(GURL(kURL), kTitle);
  manager()->Add(GURL(kURL1), kTitle1);
  EXPECT_EQ(2u, manager()->size());

  // Search with a multi-word query text.
  std::vector<const BookmarkNode*> results;
  bookmarks::QueryFields query;
  query.word_phrase_query.reset(
      new base::string16(base::ASCIIToUTF16("dog cat")));
  manager()->GetMatchingNodes(query, 5, &results);
  EXPECT_EQ(1u, results.size());

  // Search with a single word query text.
  results.clear();
  query.word_phrase_query.reset(new base::string16(base::ASCIIToUTF16("dog")));
  manager()->GetMatchingNodes(query, 5, &results);
  EXPECT_EQ(2u, results.size());

  // Search with empty string. Shouldn't match anything.
  results.clear();
  query.word_phrase_query.reset(new base::string16());
  manager()->GetMatchingNodes(query, 5, &results);
  EXPECT_EQ(0u, results.size());
}

TEST_F(ReadingListManagerImplTest, GetMatchingNodesWithMaxCount) {
  manager()->Add(GURL(kURL), kTitle);
  manager()->Add(GURL(kURL1), kTitle1);
  EXPECT_EQ(2u, manager()->size());

  // Search with a query text.
  std::vector<const BookmarkNode*> results;
  bookmarks::QueryFields query;
  query.word_phrase_query.reset(new base::string16(base::ASCIIToUTF16("dog")));
  manager()->GetMatchingNodes(query, 5, &results);
  EXPECT_EQ(2u, results.size());

  // Search with having pre-existing elements in |results|.
  manager()->GetMatchingNodes(query, 5, &results);
  EXPECT_EQ(4u, results.size());

  // Max count should never be exceeded.
  manager()->GetMatchingNodes(query, 5, &results);
  EXPECT_EQ(5u, results.size());
  manager()->GetMatchingNodes(query, 5, &results);
  EXPECT_EQ(5u, results.size());
}

// If Add() the same URL twice, the first bookmark node pointer will be
// invalidated.
TEST_F(ReadingListManagerImplTest, AddTwice) {
  // Adds a node twice.
  GURL url(kURL);
  Add(url, kTitle);
  const auto* new_node = Add(url, kTitle1);
  EXPECT_EQ(kTitle1, base::UTF16ToUTF8(new_node->GetTitle()));
  EXPECT_EQ(url, new_node->url());
}

// If Add() with an invalid title, nullptr will be returned.
TEST_F(ReadingListManagerImplTest, AddInvalidTitle) {
  GURL url(kURL);

  // Use an invalid UTF8 string.
  base::string16 dummy;
  EXPECT_FALSE(
      base::UTF8ToUTF16(kInvalidUTF8, base::size(kInvalidUTF8), &dummy));
  const auto* new_node = Add(url, std::string(kInvalidUTF8));
  EXPECT_EQ(nullptr, new_node)
      << "Should return nullptr when failed to parse the title.";
}

// If Add() with an invalid URL, nullptr will be returned.
TEST_F(ReadingListManagerImplTest, AddInvalidURL) {
  GURL invalid_url("chrome://flags");
  EXPECT_FALSE(reading_list_model()->IsUrlSupported(invalid_url));

  // Use an invalid URL, the observer method ReadingListDidAddEntry() won't be
  // invoked.
  const auto* new_node = manager()->Add(invalid_url, kTitle);
  EXPECT_EQ(nullptr, new_node)
      << "Should return nullptr when the URL scheme is not supported.";
}

// Verifes SetReadStatus()/GetReadStatus() API.
TEST_F(ReadingListManagerImplTest, ReadStatus) {
  GURL url(kURL);

  // No op when no reading list entries.
  manager()->SetReadStatus(url, true);
  EXPECT_EQ(0u, manager()->size());

  // Add a node and mark as read.
  Add(url, kTitle);
  SetReadStatus(url, true);

  const BookmarkNode* node = manager()->Get(url);
  ASSERT_TRUE(node);
  EXPECT_EQ(url, node->url());
  std::string read_status;
  node->GetMetaInfo(kReadStatusKey, &read_status);
  EXPECT_EQ(kReadStatusRead, read_status);
  EXPECT_EQ(0u, manager()->unread_size());
  EXPECT_TRUE(manager()->GetReadStatus(node));

  // Mark as unread.
  SetReadStatus(url, false);
  node = manager()->Get(url);
  node->GetMetaInfo(kReadStatusKey, &read_status);
  EXPECT_EQ(kReadStatusUnread, read_status);
  EXPECT_EQ(1u, manager()->unread_size());
  EXPECT_FALSE(manager()->GetReadStatus(node));

  // Node not in the reading list should return false.
  auto other_node =
      std::make_unique<BookmarkNode>(0, base::GUID::GenerateRandomV4(), url);
  EXPECT_FALSE(manager()->GetReadStatus(node));

  // Root node should return false.
  EXPECT_FALSE(manager()->GetReadStatus(manager()->GetRoot()));
}

// Verifies the bookmark node is added when sync or other source adds the
// reading list entry from |reading_list_model_|.
TEST_F(ReadingListManagerImplTest, ReadingListDidAddEntry) {
  GURL url(kURL);
  EXPECT_CALL(*observer(), ReadingListChanged()).RetiresOnSaturation();
  reading_list_model()->AddEntry(url, kTitle, reading_list::ADDED_VIA_SYNC);

  const auto* node = manager()->Get(url);
  EXPECT_TRUE(node);
  EXPECT_EQ(url, node->url());
  EXPECT_EQ(1u, manager()->size());
}

// Verifies the bookmark node is deleted when sync or other source deletes the
// reading list entry from |reading_list_model_|.
TEST_F(ReadingListManagerImplTest, ReadingListWillRemoveEntry) {
  GURL url(kURL);

  // Adds a node.
  const auto* node = Add(url, kTitle);
  EXPECT_TRUE(node);
  EXPECT_EQ(url, node->url());
  EXPECT_EQ(1u, manager()->size());

  // Removes it from |reading_list_model_|.
  EXPECT_CALL(*observer(), ReadingListChanged()).RetiresOnSaturation();
  reading_list_model()->RemoveEntryByURL(url);
  node = manager()->Get(url);
  EXPECT_FALSE(node);
  EXPECT_EQ(0u, manager()->size());
}

// Verifies the bookmark node is updated when sync or other source updates the
// reading list entry from |reading_list_model_|.
TEST_F(ReadingListManagerImplTest, ReadingListWillMoveEntry) {
  GURL url(kURL);

  // Adds a node.
  const auto* node = Add(url, kTitle);
  EXPECT_TRUE(node);
  EXPECT_FALSE(manager()->GetReadStatus(node));

  SetReadStatus(url, true);
  EXPECT_TRUE(manager()->GetReadStatus(node));
}

}  // namespace
