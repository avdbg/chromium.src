// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/hfs.h"

#include <libkern/OSByteOrder.h>
#include <stddef.h>
#include <sys/stat.h>

#include <map>
#include <set>
#include <vector>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/utility/safe_browsing/mac/convert_big_endian.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"

namespace safe_browsing {
namespace dmg {

// UTF-16 character for file path seprator.
static const base::char16 kFilePathSeparator = '/';

static void ConvertBigEndian(HFSPlusForkData* fork) {
  ConvertBigEndian(&fork->logicalSize);
  ConvertBigEndian(&fork->clumpSize);
  ConvertBigEndian(&fork->totalBlocks);
  for (size_t i = 0; i < base::size(fork->extents); ++i) {
    ConvertBigEndian(&fork->extents[i].startBlock);
    ConvertBigEndian(&fork->extents[i].blockCount);
  }
}

static void ConvertBigEndian(HFSPlusVolumeHeader* header) {
  ConvertBigEndian(&header->signature);
  ConvertBigEndian(&header->version);
  ConvertBigEndian(&header->attributes);
  ConvertBigEndian(&header->lastMountedVersion);
  ConvertBigEndian(&header->journalInfoBlock);
  ConvertBigEndian(&header->createDate);
  ConvertBigEndian(&header->modifyDate);
  ConvertBigEndian(&header->backupDate);
  ConvertBigEndian(&header->checkedDate);
  ConvertBigEndian(&header->fileCount);
  ConvertBigEndian(&header->folderCount);
  ConvertBigEndian(&header->blockSize);
  ConvertBigEndian(&header->totalBlocks);
  ConvertBigEndian(&header->freeBlocks);
  ConvertBigEndian(&header->nextAllocation);
  ConvertBigEndian(&header->rsrcClumpSize);
  ConvertBigEndian(&header->dataClumpSize);
  ConvertBigEndian(&header->nextCatalogID);
  ConvertBigEndian(&header->writeCount);
  ConvertBigEndian(&header->encodingsBitmap);
  ConvertBigEndian(&header->allocationFile);
  ConvertBigEndian(&header->extentsFile);
  ConvertBigEndian(&header->catalogFile);
  ConvertBigEndian(&header->attributesFile);
  ConvertBigEndian(&header->startupFile);
}

static void ConvertBigEndian(BTHeaderRec* header) {
  ConvertBigEndian(&header->treeDepth);
  ConvertBigEndian(&header->rootNode);
  ConvertBigEndian(&header->leafRecords);
  ConvertBigEndian(&header->firstLeafNode);
  ConvertBigEndian(&header->lastLeafNode);
  ConvertBigEndian(&header->nodeSize);
  ConvertBigEndian(&header->maxKeyLength);
  ConvertBigEndian(&header->totalNodes);
  ConvertBigEndian(&header->freeNodes);
  ConvertBigEndian(&header->reserved1);
  ConvertBigEndian(&header->clumpSize);
  ConvertBigEndian(&header->attributes);
}

static void ConvertBigEndian(BTNodeDescriptor* node) {
  ConvertBigEndian(&node->fLink);
  ConvertBigEndian(&node->bLink);
  ConvertBigEndian(&node->numRecords);
}

static void ConvertBigEndian(HFSPlusCatalogFolder* folder) {
  ConvertBigEndian(&folder->recordType);
  ConvertBigEndian(&folder->flags);
  ConvertBigEndian(&folder->valence);
  ConvertBigEndian(&folder->folderID);
  ConvertBigEndian(&folder->createDate);
  ConvertBigEndian(&folder->contentModDate);
  ConvertBigEndian(&folder->attributeModDate);
  ConvertBigEndian(&folder->accessDate);
  ConvertBigEndian(&folder->backupDate);
  ConvertBigEndian(&folder->bsdInfo.ownerID);
  ConvertBigEndian(&folder->bsdInfo.groupID);
  ConvertBigEndian(&folder->bsdInfo.fileMode);
  ConvertBigEndian(&folder->textEncoding);
  ConvertBigEndian(&folder->folderCount);
}

static void ConvertBigEndian(HFSPlusCatalogFile* file) {
  ConvertBigEndian(&file->recordType);
  ConvertBigEndian(&file->flags);
  ConvertBigEndian(&file->reserved1);
  ConvertBigEndian(&file->fileID);
  ConvertBigEndian(&file->createDate);
  ConvertBigEndian(&file->contentModDate);
  ConvertBigEndian(&file->attributeModDate);
  ConvertBigEndian(&file->accessDate);
  ConvertBigEndian(&file->backupDate);
  ConvertBigEndian(&file->bsdInfo.ownerID);
  ConvertBigEndian(&file->bsdInfo.groupID);
  ConvertBigEndian(&file->bsdInfo.fileMode);
  ConvertBigEndian(&file->userInfo.fdType);
  ConvertBigEndian(&file->userInfo.fdCreator);
  ConvertBigEndian(&file->userInfo.fdFlags);
  ConvertBigEndian(&file->textEncoding);
  ConvertBigEndian(&file->reserved2);
  ConvertBigEndian(&file->dataFork);
  ConvertBigEndian(&file->resourceFork);
}

// A ReadStream implementation for an HFS+ fork. This only consults the eight
// fork extents. This does not consult the extent overflow file.
class HFSForkReadStream : public ReadStream {
 public:
  HFSForkReadStream(HFSIterator* hfs, const HFSPlusForkData& fork);
  ~HFSForkReadStream() override;

  bool Read(uint8_t* buffer, size_t buffer_size, size_t* bytes_read) override;
  // Seek only supports SEEK_SET.
  off_t Seek(off_t offset, int whence) override;

 private:
  HFSIterator* const hfs_;  // The HFS+ iterator.
  const HFSPlusForkData fork_;  // The fork to be read.
  uint8_t current_extent_;  // The current extent index in the fork.
  bool read_current_extent_;  // Whether the current_extent_ has been read.
  std::vector<uint8_t> current_extent_data_;  // Data for |current_extent_|.
  size_t fork_logical_offset_;  // The logical offset into the fork.

  DISALLOW_COPY_AND_ASSIGN(HFSForkReadStream);
};

// HFSBTreeIterator iterates over the HFS+ catalog file.
class HFSBTreeIterator {
 public:
  struct Entry {
    uint16_t record_type;  // Catalog folder item type.
    base::string16 path;  // Full path to the item.
    bool unexported;  // Whether this is HFS+ private data.
    union {
      HFSPlusCatalogFile* file;
      HFSPlusCatalogFolder* folder;
    };
  };

  HFSBTreeIterator();
  ~HFSBTreeIterator();

  bool Init(ReadStream* stream);

  bool HasNext();
  bool Next();

  const Entry* current_record() const { return &current_record_; }

 private:
  // Seeks |stream_| to the catalog node ID.
  bool SeekToNode(uint32_t node_id);

  // If required, reads the current leaf into |leaf_data_| and updates the
  // buffer offsets.
  bool ReadCurrentLeaf();

  // Returns a pointer to data at |current_leaf_offset_| in |leaf_data_|. This
  // then advances the offset by the size of the object being returned.
  template <typename T> T* GetLeafData();

  // Checks if the HFS+ catalog key is a Mac OS X reserved key that should not
  // have it or its contents iterated over.
  bool IsKeyUnexported(const base::string16& path);

  ReadStream* stream_;  // The stream backing the catalog file.
  BTHeaderRec header_;  // The header B-tree node.

  // Maps CNIDs to their full path. This is used to construct full paths for
  // items that descend from the folders in this map.
  std::map<uint32_t, base::string16> folder_cnid_map_;

  // CNIDs of the non-exported folders reserved by OS X. If an item has this
  // CNID as a parent, it should be skipped.
  std::set<uint32_t> unexported_parents_;

  // The total number of leaf records read from all the leaf nodes.
  uint32_t leaf_records_read_;

  // The number of records read from the current leaf node.
  uint32_t current_leaf_records_read_;
  uint32_t current_leaf_number_;  // The node ID of the leaf being read.
  // Whether the |current_leaf_number_|'s data has been read into the
  // |leaf_data_| buffer.
  bool read_current_leaf_;
  // The node data for |current_leaf_number_| copied from |stream_|.
  std::vector<uint8_t> leaf_data_;
  size_t current_leaf_offset_;  // The offset in |leaf_data_|.

  // Pointer to |leaf_data_| as a BTNodeDescriptor.
  const BTNodeDescriptor* current_leaf_;
  Entry current_record_;  // The record read at |current_leaf_offset_|.

  // Constant, string16 versions of the __APPLE_API_PRIVATE values.
  const base::string16 kHFSMetadataFolder =
      base::UTF8ToUTF16(base::StringPiece("\x0\x0\x0\x0HFS+ Private Data", 21));
  const base::string16 kHFSDirMetadataFolder =
      base::UTF8ToUTF16(".HFS+ Private Directory Data\xd");

  DISALLOW_COPY_AND_ASSIGN(HFSBTreeIterator);
};

HFSIterator::HFSIterator(ReadStream* stream)
    : stream_(stream),
      volume_header_() {
}

HFSIterator::~HFSIterator() {}

bool HFSIterator::Open() {
  if (stream_->Seek(1024, SEEK_SET) != 1024)
    return false;

  if (!stream_->ReadType(&volume_header_)) {
    DLOG(ERROR) << "Failed to read volume header";
    return false;
  }
  ConvertBigEndian(&volume_header_);

  if (volume_header_.signature != kHFSPlusSigWord &&
      volume_header_.signature != kHFSXSigWord) {
    DLOG(ERROR) << "Unrecognized volume header signature "
                << volume_header_.signature;
    return false;
  }

  if (volume_header_.blockSize == 0) {
    DLOG(ERROR) << "Invalid volume header block size "
                << volume_header_.blockSize;
    return false;
  }

  if (!ReadCatalogFile())
    return false;

  return true;
}

bool HFSIterator::Next() {
  if (!catalog_->HasNext())
    return false;

  // The iterator should only stop on file and folders, skipping over "thread
  // records". In addition, unexported private files and directories should be
  // skipped as well.
  bool keep_going = false;
  do {
    keep_going = catalog_->Next();
    if (keep_going) {
      if (!catalog_->current_record()->unexported &&
          (catalog_->current_record()->record_type == kHFSPlusFolderRecord ||
           catalog_->current_record()->record_type == kHFSPlusFileRecord)) {
        return true;
      }
      keep_going = catalog_->HasNext();
    }
  } while (keep_going);

  return keep_going;
}

bool HFSIterator::IsDirectory() {
  return catalog_->current_record()->record_type == kHFSPlusFolderRecord;
}

bool HFSIterator::IsSymbolicLink() {
  if (IsDirectory())
    return S_ISLNK(catalog_->current_record()->folder->bsdInfo.fileMode);
  else
    return S_ISLNK(catalog_->current_record()->file->bsdInfo.fileMode);
}

bool HFSIterator::IsHardLink() {
  if (IsDirectory())
    return false;
  const HFSPlusCatalogFile* file = catalog_->current_record()->file;
  return file->userInfo.fdType == kHardLinkFileType &&
         file->userInfo.fdCreator == kHFSPlusCreator;
}

bool HFSIterator::IsDecmpfsCompressed() {
  if (IsDirectory())
    return false;
  const HFSPlusCatalogFile* file = catalog_->current_record()->file;
  return file->bsdInfo.ownerFlags & UF_COMPRESSED;
}

base::string16 HFSIterator::GetPath() {
  return catalog_->current_record()->path;
}

std::unique_ptr<ReadStream> HFSIterator::GetReadStream() {
  if (IsDirectory() || IsHardLink())
    return nullptr;

  DCHECK_EQ(kHFSPlusFileRecord, catalog_->current_record()->record_type);
  return std::make_unique<HFSForkReadStream>(
      this, catalog_->current_record()->file->dataFork);
}

bool HFSIterator::SeekToBlock(uint64_t block) {
  uint64_t offset = block * volume_header_.blockSize;
  off_t rv = stream_->Seek(offset, SEEK_SET);
  return rv >= 0 && static_cast<uint64_t>(rv) == offset;
}

bool HFSIterator::ReadCatalogFile() {
  catalog_file_.reset(new HFSForkReadStream(this, volume_header_.catalogFile));
  catalog_.reset(new HFSBTreeIterator());
  return catalog_->Init(catalog_file_.get());
}

HFSForkReadStream::HFSForkReadStream(HFSIterator* hfs,
                                     const HFSPlusForkData& fork)
    : hfs_(hfs),
      fork_(fork),
      current_extent_(0),
      read_current_extent_(false),
      current_extent_data_(),
      fork_logical_offset_(0) {
}

HFSForkReadStream::~HFSForkReadStream() {}

bool HFSForkReadStream::Read(uint8_t* buffer,
                             size_t buffer_size,
                             size_t* bytes_read) {
  size_t buffer_space_remaining = buffer_size;
  *bytes_read = 0;

  if (fork_logical_offset_ == fork_.logicalSize)
    return true;

  for (; current_extent_ < base::size(fork_.extents); ++current_extent_) {
    // If the buffer is out of space, do not attempt any reads. Check this
    // here, so that current_extent_ is advanced by the loop if the last
    // extent was fully read.
    if (buffer_space_remaining == 0)
      break;

    const HFSPlusExtentDescriptor* extent = &fork_.extents[current_extent_];

    // A zero-length extent means end-of-fork.
    if (extent->startBlock == 0 && extent->blockCount == 0)
      break;

    auto extent_size =
        base::CheckedNumeric<size_t>(extent->blockCount) * hfs_->block_size();
    if (extent_size.ValueOrDefault(0) == 0) {
      DLOG(ERROR) << "Extent blockCount overflows or is 0";
      return false;
    }

    // Read the entire extent now, to avoid excessive seeking and re-reading.
    if (!read_current_extent_) {
      if (!hfs_->SeekToBlock(extent->startBlock)) {
        DLOG(ERROR) << "Failed to seek to block " << extent->startBlock;
        return false;
      }
      current_extent_data_.resize(extent_size.ValueOrDie());
      if (!hfs_->stream()->ReadExact(current_extent_data_.data(),
                                     extent_size.ValueOrDie())) {
        DLOG(ERROR) << "Failed to read extent " << current_extent_;
        return false;
      }

      read_current_extent_ = true;
    }

    size_t extent_offset = (fork_logical_offset_ % extent_size).ValueOrDie();
    size_t bytes_to_copy = std::min(
        std::min(
            static_cast<size_t>(fork_.logicalSize) - fork_logical_offset_,
            static_cast<size_t>((extent_size - extent_offset).ValueOrDie())),
        buffer_space_remaining);

    memcpy(&buffer[buffer_size - buffer_space_remaining],
           &current_extent_data_[extent_offset],
           bytes_to_copy);

    buffer_space_remaining -= bytes_to_copy;
    *bytes_read += bytes_to_copy;
    fork_logical_offset_ += bytes_to_copy;

    // If the fork's data have been read, then end the loop.
    if (fork_logical_offset_ == fork_.logicalSize)
      return true;

    // If this extent still has data to be copied out, then the read was
    // partial and the buffer is full. Do not advance to the next extent.
    if (extent_offset < current_extent_data_.size())
      break;

    // Advance to the next extent, so reset the state.
    read_current_extent_ = false;
  }

  return true;
}

off_t HFSForkReadStream::Seek(off_t offset, int whence) {
  DCHECK_EQ(SEEK_SET, whence);
  DCHECK_GE(offset, 0);
  DCHECK(offset == 0 || static_cast<uint64_t>(offset) < fork_.logicalSize);
  size_t target_block = offset / hfs_->block_size();
  size_t block_count = 0;
  for (size_t i = 0; i < base::size(fork_.extents); ++i) {
    const HFSPlusExtentDescriptor* extent = &fork_.extents[i];

    // An empty extent indicates end-of-fork.
    if (extent->startBlock == 0 && extent->blockCount == 0)
      break;

    base::CheckedNumeric<size_t> new_block_count(block_count);
    new_block_count += extent->blockCount;
    if (!new_block_count.IsValid()) {
      DLOG(ERROR) << "Seek offset block count overflows";
      return false;
    }

    if (target_block < new_block_count.ValueOrDie()) {
      if (current_extent_ != i) {
        read_current_extent_ = false;
        current_extent_ = i;
      }
      auto iterator_block_offset =
          base::CheckedNumeric<size_t>(block_count) * hfs_->block_size();
      if (!iterator_block_offset.IsValid()) {
        DLOG(ERROR) << "Seek block offset overflows";
        return false;
      }
      fork_logical_offset_ = offset;
      return offset;
    }

    block_count = new_block_count.ValueOrDie();
  }
  return -1;
}

HFSBTreeIterator::HFSBTreeIterator()
    : stream_(),
      header_(),
      leaf_records_read_(0),
      current_leaf_records_read_(0),
      current_leaf_number_(0),
      read_current_leaf_(false),
      leaf_data_(),
      current_leaf_offset_(0),
      current_leaf_() {
}

HFSBTreeIterator::~HFSBTreeIterator() {}

bool HFSBTreeIterator::Init(ReadStream* stream) {
  DCHECK(!stream_);
  stream_ = stream;

  if (stream_->Seek(0, SEEK_SET) != 0) {
    DLOG(ERROR) << "Failed to seek to header node";
    return false;
  }

  BTNodeDescriptor node;
  if (!stream_->ReadType(&node)) {
    DLOG(ERROR) << "Failed to read BTNodeDescriptor";
    return false;
  }
  ConvertBigEndian(&node);

  if (node.kind != kBTHeaderNode) {
    DLOG(ERROR) << "Initial node is not a header node";
    return false;
  }

  if (!stream_->ReadType(&header_)) {
    DLOG(ERROR) << "Failed to read BTHeaderRec";
    return false;
  }
  ConvertBigEndian(&header_);

  if (header_.nodeSize < sizeof(BTNodeDescriptor)) {
    DLOG(ERROR) << "Invalid header: node size smaller than BTNodeDescriptor";
    return false;
  }

  current_leaf_number_ = header_.firstLeafNode;
  leaf_data_.resize(header_.nodeSize);

  return true;
}

bool HFSBTreeIterator::HasNext() {
  return leaf_records_read_ < header_.leafRecords;
}

bool HFSBTreeIterator::Next() {
  if (!ReadCurrentLeaf())
    return false;

  GetLeafData<uint16_t>();  // keyLength

  uint32_t parent_id;
  if (auto* parent_id_ptr = GetLeafData<uint32_t>()) {
    parent_id = OSSwapBigToHostInt32(*parent_id_ptr);
  } else {
    return false;
  }

  uint16_t key_string_length;
  if (auto* key_string_length_ptr = GetLeafData<uint16_t>()) {
    key_string_length = OSSwapBigToHostInt16(*key_string_length_ptr);
  } else {
    return false;
  }

  // Read and byte-swap the variable-length key string.
  base::string16 key(key_string_length, '\0');
  for (uint16_t i = 0; i < key_string_length; ++i) {
    auto* character = GetLeafData<uint16_t>();
    if (!character) {
      DLOG(ERROR) << "Key string length points past leaf data";
      return false;
    }
    key[i] = OSSwapBigToHostInt16(*character);
  }

  // Read the record type and then rewind as the field is part of the catalog
  // structure that is read next.
  auto* record_type = GetLeafData<int16_t>();
  if (!record_type) {
    DLOG(ERROR) << "Failed to read record type";
    return false;
  }
  current_record_.record_type = OSSwapBigToHostInt16(*record_type);
  current_record_.unexported = false;
  current_leaf_offset_ -= sizeof(int16_t);
  switch (current_record_.record_type) {
    case kHFSPlusFolderRecord: {
      auto* folder = GetLeafData<HFSPlusCatalogFolder>();
      ConvertBigEndian(folder);
      ++leaf_records_read_;
      ++current_leaf_records_read_;

      // If this key is unexported, or the parent folder is, then mark the
      // record as such.
      if (IsKeyUnexported(key) ||
          unexported_parents_.find(parent_id) != unexported_parents_.end()) {
        unexported_parents_.insert(folder->folderID);
        current_record_.unexported = true;
      }

      // Update the CNID map to construct the path tree.
      if (parent_id != 0) {
        auto parent_name = folder_cnid_map_.find(parent_id);
        if (parent_name != folder_cnid_map_.end())
          key = parent_name->second + kFilePathSeparator + key;
      }
      folder_cnid_map_[folder->folderID] = key;

      current_record_.path = key;
      current_record_.folder = folder;
      break;
    }
    case kHFSPlusFileRecord: {
      auto* file = GetLeafData<HFSPlusCatalogFile>();
      ConvertBigEndian(file);
      ++leaf_records_read_;
      ++current_leaf_records_read_;

      base::string16 path =
          folder_cnid_map_[parent_id] + kFilePathSeparator + key;
      current_record_.path = path;
      current_record_.file = file;
      current_record_.unexported =
          unexported_parents_.find(parent_id) != unexported_parents_.end();
      break;
    }
    case kHFSPlusFolderThreadRecord:
    case kHFSPlusFileThreadRecord: {
      // Thread records are used to quickly locate a file or folder just by
      // CNID. As these are not necessary for the iterator, skip past the data.
      GetLeafData<uint16_t>();  // recordType
      GetLeafData<uint16_t>();  // reserved
      GetLeafData<uint32_t>();  // parentID
      auto string_length = OSSwapBigToHostInt16(*GetLeafData<uint16_t>());
      for (uint16_t i = 0; i < string_length; ++i)
        GetLeafData<uint16_t>();
      ++leaf_records_read_;
      ++current_leaf_records_read_;
      break;
    }
    default:
      DLOG(ERROR) << "Unknown record type " << current_record_.record_type;
      return false;
  }

  // If all the records from this leaf have been read, follow the forward link
  // to the next B-Tree leaf node.
  if (current_leaf_records_read_ >= current_leaf_->numRecords) {
    current_leaf_number_ = current_leaf_->fLink;
    read_current_leaf_ = false;
  }

  return true;
}

bool HFSBTreeIterator::SeekToNode(uint32_t node_id) {
  if (node_id >= header_.totalNodes)
    return false;
  size_t offset = node_id * header_.nodeSize;
  if (stream_->Seek(offset, SEEK_SET) != -1) {
    current_leaf_number_ = node_id;
    return true;
  }
  return false;
}

bool HFSBTreeIterator::ReadCurrentLeaf() {
  if (read_current_leaf_)
    return true;

  if (!SeekToNode(current_leaf_number_)) {
    DLOG(ERROR) << "Failed to seek to node " << current_leaf_number_;
    return false;
  }

  if (!stream_->ReadExact(&leaf_data_[0], header_.nodeSize)) {
    DLOG(ERROR) << "Failed to read node " << current_leaf_number_;
    return false;
  }

  auto* leaf = reinterpret_cast<BTNodeDescriptor*>(&leaf_data_[0]);
  ConvertBigEndian(leaf);
  if (leaf->kind != kBTLeafNode) {
    DLOG(ERROR) << "Node " << current_leaf_number_ << " is not a leaf";
    return false;
  }
  current_leaf_ = leaf;
  current_leaf_offset_ = sizeof(BTNodeDescriptor);
  current_leaf_records_read_ = 0;
  read_current_leaf_ = true;
  return true;
}

template <typename T>
T* HFSBTreeIterator::GetLeafData() {
  base::CheckedNumeric<size_t> size = sizeof(T);
  auto new_offset = size + current_leaf_offset_;
  if (!new_offset.IsValid() || new_offset.ValueOrDie() >= leaf_data_.size())
    return nullptr;
  T* object = reinterpret_cast<T*>(&leaf_data_[current_leaf_offset_]);
  current_leaf_offset_ = new_offset.ValueOrDie();
  return object;
}

bool HFSBTreeIterator::IsKeyUnexported(const base::string16& key) {
  return key == kHFSDirMetadataFolder ||
         key == kHFSMetadataFolder;
}

}  // namespace dmg
}  // namespace safe_browsing
