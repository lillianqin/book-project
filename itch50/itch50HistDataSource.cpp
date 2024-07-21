#include "itch50HistDataSource.h"
#include "itch50.h"
#include <absl/cleanup/cleanup.h>
#include <absl/log/log.h>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bookproj {
namespace datasource {

HistDataSource::Timestamp Itch50HistDataSource::midnightNYTime(int date) {
  auto tz = ::getenv("TZ");
  ::setenv("TZ", ":America/New_York", 1);
  ::tzset();

  std::tm timeinfo = std::tm();
  timeinfo.tm_year = date / 10000 - 1900;
  timeinfo.tm_mon = date / 100 % 100 - 1;
  timeinfo.tm_mday = date % 100;
  timeinfo.tm_isdst = -1;

  std::time_t tt = std::mktime(&timeinfo);
  HistDataSource::Timestamp midnight = std::chrono::system_clock::from_time_t(tt);

  if (tz) {
    ::setenv("TZ", tz, 1);
  } else {
    ::unsetenv("TZ");
  }
  ::tzset();
  return midnight;
}

Itch50HistDataSource::Itch50HistDataSource(int date) : midnight_(midnightNYTime(date)) {
  // open and mmap the file, throw if anything fails
  std::string filename = rootPath_ + '/' + "nasdaq_itch." + std::to_string(date) + ".dat";
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    throw std::runtime_error("Failed to open file " + filename + ": " + strerror(errno));
  }

  absl::Cleanup file_closer = [fd] { close(fd); };
  struct stat sb;
  if (fstat(fd, &sb) != 0) {
    throw std::runtime_error("Error stating file " + filename + ": " + strerror(errno));
  }

  void *mapped = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapped == MAP_FAILED) {
    throw std::runtime_error("Error mmapping file " + filename + ": " + strerror(errno));
  }

  madvise(mapped, sb.st_size, MADV_SEQUENTIAL);
  data_ = reinterpret_cast<std::byte *>(mapped);
  totalSize_ = sb.st_size;

  // update nextTime and nextMessage
  Itch50HistDataSource::advance();
}

Itch50HistDataSource::~Itch50HistDataSource() {
  if (unmappedSize_ < totalSize_) {
    munmap(data_ + unmappedSize_, totalSize_ - unmappedSize_);
  }
}

Itch50HistDataSource::Timestamp Itch50HistDataSource::seek(Timestamp time) {
  while (nextTime_ < time) {
    Itch50HistDataSource::advance();
  }
  return nextTime_;
}

Itch50HistDataSource::Timestamp Itch50HistDataSource::advance() {
  if (!nextMessage_.empty()) [[likely]] {
    // 2 bytes are the big-endian size header in raw file
    currentOffset_ += 2 + nextMessage_.size();
    if (currentOffset_ >= unmappedSize_ + CHUNK_SIZE) [[unlikely]] {
      size_t unmapSz = (currentOffset_ - unmappedSize_) / CHUNK_SIZE * CHUNK_SIZE;
      munmap(data_ + unmappedSize_, unmapSz);
      unmappedSize_ += unmapSz;
    }
  }

  // format is 2-byte bid-endian size followed by message of size bytes
  const size_t msgStart = currentOffset_ + 2;
  if (msgStart < totalSize_) [[likely]] {
    size_t msgSize = std::to_integer<size_t>(data_[currentOffset_]) * 256 |
                     std::to_integer<size_t>(data_[currentOffset_ + 1]);
    if ((msgSize >= sizeof(itch50::CommonHeader)) & (msgStart + msgSize <= totalSize_))
        [[likely]] {
      // peak at time first, which is nanoseconds offset from midnight_
      const itch50::CommonHeader *header =
          std::launder(reinterpret_cast<const itch50::CommonHeader *>(data_ + msgStart));
      nextTime_ =
          midnight_ + std::chrono::nanoseconds(itch50::nanosSinceMidnight(header->timestamp));
      if (nextTime_ <= endTime_) [[likely]] {
        nextMessage_ = std::span(data_ + msgStart, msgSize);
        return nextTime_;
      } else {
        // reached endTime, update currentOffset_ to suppress error below
        currentOffset_ = totalSize_;
      }
    }
  }

  // we have reached file end, or reached endTime, or errored out
  if (currentOffset_ != totalSize_) {
    LOG(ERROR) << "Itch50HistDataSource file is not well formatted or truncated, read "
               << currentOffset_ << " out of " << totalSize_ << " bytes";
    currentOffset_ = totalSize_;
  }
  nextTime_ = Timestamp::max();
  nextMessage_ = {};
  return nextTime_;
}

std::string Itch50HistDataSource::rootPath_;
void Itch50HistDataSource::setRootPath(const std::string &rootPath) { rootPath_ = rootPath; }

} // namespace datasource
} // namespace bookproj