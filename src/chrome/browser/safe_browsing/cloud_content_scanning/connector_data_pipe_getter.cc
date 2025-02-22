// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/connector_data_pipe_getter.h"

#include <algorithm>

#include "base/files/memory_mapped_file.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "mojo/public/c/system/data_pipe.h"
#include "net/base/net_errors.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/mman.h>

#include "base/threading/scoped_blocking_call.h"
#endif

namespace safe_browsing {

namespace {
const char kDataContentType[] = "Content-Type: application/octet-stream";

// Write the data from |file_| by chunks of 32 kbs.
constexpr int64_t kMaxSize = 32 * 1024;

}  // namespace

#if BUILDFLAG(IS_POSIX)
bool ConnectorDataPipeGetter::InternalMemoryMappedFile::Initialize(
    base::File file) {
  if (IsValid())
    return false;

  file_ = std::move(file);

  if (!DoInitialize()) {
    CloseHandles();
    return false;
  }

  return true;
}

bool ConnectorDataPipeGetter::InternalMemoryMappedFile::DoInitialize() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  int64_t file_len = file_.GetLength();
  if (file_len < 0) {
    DPLOG(ERROR) << "fstat " << file_.GetPlatformFile();
    return false;
  }
  if (!base::IsValueInRangeForNumericType<size_t>(file_len)) {
    return false;
  }
  void* mapped = mmap(nullptr, static_cast<size_t>(file_len), PROT_READ,
                      MAP_SHARED, file_.GetPlatformFile(), 0);
  if (mapped == MAP_FAILED) {
    // Retry with MAP_PRIVATE mode.
    // Some file systems do not support MAP_SHARED. Here, it is acceptable to
    // use MAP_PRIVATE instead. Note: For MAP_PRIVATE, it is unspecified whether
    // changes to the underlying file are carried through to the mapped region
    // after the mmap call.
    mapped = mmap(nullptr, static_cast<size_t>(file_len), PROT_READ,
                  MAP_PRIVATE, file_.GetPlatformFile(), 0);
  }
  if (mapped == MAP_FAILED) {
    DPLOG(ERROR) << "Upload failure: The creation of a memory mapped file "
                    "failed for file "
                 << file_.GetPlatformFile();
    return false;
  }
  length_ = static_cast<size_t>(file_len);
  data_ = static_cast<uint8_t*>(mapped);
  return true;
}

void ConnectorDataPipeGetter::InternalMemoryMappedFile::CloseHandles() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (data_) {
    munmap(data_, length_);
  }
  data_ = nullptr;
  length_ = 0;
  file_.Close();
}

#endif  // BUILDFLAG(IS_POSIX)

// static
std::unique_ptr<ConnectorDataPipeGetter>
ConnectorDataPipeGetter::CreateMultipartPipeGetter(const std::string& boundary,
                                                   const std::string& metadata,
                                                   base::File file) {
  if (!file.IsValid()) {
    return nullptr;
  }

  auto mm_file = std::make_unique<InternalMemoryMappedFile>();
  if (!mm_file->Initialize(std::move(file))) {
    return nullptr;
  }

  return std::make_unique<ConnectorDataPipeGetter>(boundary, metadata,
                                                   std::move(mm_file));
}

// static
std::unique_ptr<ConnectorDataPipeGetter>
ConnectorDataPipeGetter::CreateMultipartPipeGetter(
    const std::string& boundary,
    const std::string& metadata,
    base::ReadOnlySharedMemoryRegion page) {
  if (!page.IsValid()) {
    return nullptr;
  }

  base::ReadOnlySharedMemoryMapping mapping = page.Map();
  if (!mapping.IsValid()) {
    return nullptr;
  }

  return std::make_unique<ConnectorDataPipeGetter>(boundary, metadata,
                                                   std::move(mapping));
}

// static
std::unique_ptr<ConnectorDataPipeGetter>
ConnectorDataPipeGetter::CreateResumablePipeGetter(base::File file) {
  if (!file.IsValid()) {
    return nullptr;
  }

  auto mm_file = std::make_unique<InternalMemoryMappedFile>();
  if (!mm_file->Initialize(std::move(file))) {
    return nullptr;
  }

  return std::make_unique<ConnectorDataPipeGetter>(/*boundary*/ std::string(),
                                                   /*metadata*/ std::string(),
                                                   std::move(mm_file));
}

// static
std::unique_ptr<ConnectorDataPipeGetter>
ConnectorDataPipeGetter::CreateResumablePipeGetter(
    base::ReadOnlySharedMemoryRegion page) {
  if (!page.IsValid())
    return nullptr;

  base::ReadOnlySharedMemoryMapping mapping = page.Map();
  if (!mapping.IsValid())
    return nullptr;

  return std::make_unique<ConnectorDataPipeGetter>(/*boundary*/ std::string(),
                                                   /*metadata*/ std::string(),
                                                   std::move(mapping));
}

ConnectorDataPipeGetter::ConnectorDataPipeGetter(
    const std::string& boundary,
    const std::string& metadata,
    std::unique_ptr<InternalMemoryMappedFile> file)
    : ConnectorDataPipeGetter(boundary,
                              metadata,
                              std::move(file),
                              base::ReadOnlySharedMemoryMapping()) {
  file_data_pipe_ = true;
  CHECK(file_->IsValid());
}

ConnectorDataPipeGetter::ConnectorDataPipeGetter(
    const std::string& boundary,
    const std::string& metadata,
    base::ReadOnlySharedMemoryMapping page)
    : ConnectorDataPipeGetter(boundary, metadata, nullptr, std::move(page)) {
  file_data_pipe_ = false;
  CHECK(page_.IsValid());
}

ConnectorDataPipeGetter::ConnectorDataPipeGetter(
    const std::string& boundary,
    const std::string& metadata,
    std::unique_ptr<InternalMemoryMappedFile> file,
    base::ReadOnlySharedMemoryMapping page)
    : file_(std::move(file)), page_(std::move(page)) {
  if (!boundary.empty() && !metadata.empty()) {
    PrepareMultipartRequestFormat(boundary, metadata);
  }
}

ConnectorDataPipeGetter::~ConnectorDataPipeGetter() = default;

void ConnectorDataPipeGetter::Read(mojo::ScopedDataPipeProducerHandle pipe,
                                   ReadCallback callback) {
  Reset();

  std::move(callback).Run(net::OK, FullSize());

  pipe_ = std::move(pipe);
  watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  watcher_->Watch(
      pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE, MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&ConnectorDataPipeGetter::MojoReadyCallback,
                          base::Unretained(this)));

  Write();
}

void ConnectorDataPipeGetter::Clone(
    mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ConnectorDataPipeGetter::Reset() {
  watcher_.reset();
  pipe_.reset();
  write_position_ = 0;
}

std::unique_ptr<ConnectorDataPipeGetter::InternalMemoryMappedFile>
ConnectorDataPipeGetter::ReleaseFile() {
  return std::move(file_);
}

void ConnectorDataPipeGetter::MojoReadyCallback(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  Write();
}

void ConnectorDataPipeGetter::Write() {
  int64_t metadata_end = metadata_.size();
  if (IsWritePositionInRange(0, metadata_end)) {
    if (!WriteMultipartRequestFormat(metadata_, write_position_)) {
      return;
    }
  }

  int64_t data_end = metadata_end;
  if (is_file_data_pipe()) {
    data_end += file_->length();
  } else {
    DCHECK(is_page_data_pipe());
    data_end += page_.size();
  }

  if (IsWritePositionInRange(metadata_end, data_end)) {
    if (is_file_data_pipe() && !WriteFileData())
      return;
    if (is_page_data_pipe() && !WritePageData())
      return;
  }

  int64_t last_boundary_end = data_end + last_boundary_.size();
  DCHECK_EQ(last_boundary_end, FullSize());
  if (IsWritePositionInRange(data_end, last_boundary_end)) {
    int64_t offset = write_position_ - data_end;
    if (!WriteMultipartRequestFormat(last_boundary_, offset)) {
      return;
    }
  }

  if (write_position_ == FullSize()) {
    Reset();
  }
}

inline void ConnectorDataPipeGetter::PrepareMultipartRequestFormat(
    const std::string& boundary,
    const std::string& metadata) {
  metadata_ = base::StrCat({"--", boundary, "\r\n", kDataContentType,
                            "\r\n\r\n", metadata, "\r\n--", boundary, "\r\n",
                            kDataContentType, "\r\n\r\n"});

  last_boundary_ = base::StrCat({"\r\n--", boundary, "--\r\n"});
}

bool ConnectorDataPipeGetter::WriteMultipartRequestFormat(
    const std::string& str,
    int64_t offset) {
  CHECK_GE(offset, 0);
  CHECK_LT(offset, static_cast<int64_t>(str.size()));

  return Write(str.data(), str.size(), offset);
}

bool ConnectorDataPipeGetter::WriteFileData() {
  int64_t file_offset = write_position_ - metadata_.size();
  CHECK(file_->IsValid());
  CHECK_GE(file_offset, 0);
  CHECK_LT(file_offset, static_cast<int64_t>(file_->length()));

  return Write(reinterpret_cast<const char*>(file_->data()), file_->length(),
               file_offset);
}

bool ConnectorDataPipeGetter::WritePageData() {
  int64_t page_offset = write_position_ - metadata_.size();
  CHECK_GE(page_offset, 0);
  CHECK_LT(page_offset, static_cast<int64_t>(page_.size()));

  return Write(page_.GetMemoryAs<char>(), page_.size(), page_offset);
}

bool ConnectorDataPipeGetter::Write(const char* data,
                                    int64_t full_size,
                                    int64_t offset) {
  while (true) {
    int64_t remaining_bytes = full_size - offset;
    size_t write_size =
        base::checked_cast<size_t>(std::min(kMaxSize, remaining_bytes));
    if (write_size == 0) {
      // The data is fully read, so allow the next Write.
      return true;
    }

    int result =
        pipe_->WriteData(data + offset, &write_size, MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      watcher_->ArmOrNotify();
      return false;
    } else if (result != MOJO_RESULT_OK) {
      Reset();
      return false;
    }

    offset += write_size;
    write_position_ += write_size;
    DCHECK_LE(offset, full_size);
  }
}

bool ConnectorDataPipeGetter::IsWritePositionInRange(int64_t range_start,
                                                     int64_t range_end) {
  return (range_start <= write_position_ && write_position_ < range_end);
}

int64_t ConnectorDataPipeGetter::FullSize() {
  int64_t size = metadata_.size() + last_boundary_.size();
  if (is_file_data_pipe()) {
    return size + file_->length();
  } else {
    DCHECK(is_page_data_pipe());
    return size + page_.size();
  }
}

bool ConnectorDataPipeGetter::is_file_data_pipe() const {
  return file_data_pipe_;
}

bool ConnectorDataPipeGetter::is_page_data_pipe() const {
  return !file_data_pipe_;
}

}  // namespace safe_browsing
