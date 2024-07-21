#pragma once

#include "datasource/HistDataSource.h"
namespace bookproj {
namespace datasource {

class Itch50HistDataSource final : public HistDataSource {
public:
  Itch50HistDataSource(int date);
  Itch50HistDataSource(const Itch50HistDataSource &) = delete;
  Itch50HistDataSource &operator=(const Itch50HistDataSource &) = delete;

  virtual ~Itch50HistDataSource();

  virtual Timestamp seek(Timestamp time) override;
  virtual Timestamp advance() override;

  // set a end time so that no message should be delivered after the given time,
  // in other words, messages delivered are in the range of [seekTime, endTime]
  void setEndTime(Timestamp endTime) { endTime_ = endTime; }

  // return the current offset into the data file
  size_t currentOffset() const { return currentOffset_; }

  // return a timestamp that represents the midnight of the given date in NY time
  static Timestamp midnightNYTime(int date);

  // root path where data files are located, file path as rootPath/nasdaq_itch.YYYYMMDD.dat
  static void setRootPath(const std::string &rootPath);

  // string name for for HistDataSourceFactory
  static constexpr std::string name = "nasdaq_itch50";

private:
  Timestamp midnight_;
  Timestamp endTime_ = Timestamp::max();

  size_t currentOffset_ = 0;
  size_t totalSize_ = 0;
  std::byte *data_ = nullptr;

  // size that has been unmapped, must be multiple of CHUNK_SIZE below
  size_t unmappedSize_ = 0;

  static constexpr size_t CHUNK_SIZE = 1 << 22;
  static std::string rootPath_;
};

} // namespace datasource
} // namespace bookproj
