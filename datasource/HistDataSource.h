#pragma once

#include <chrono>
#include <cstddef>
#include <span>

namespace bookproj {

namespace datasource {

class HistDataSource {
public:
  HistDataSource() = default;
  HistDataSource(const HistDataSource &) = delete;
  HistDataSource &operator=(const HistDataSource &) = delete;

  virtual ~HistDataSource() = default;

  using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

  Timestamp nextTime() const { return nextTime_; }
  std::span<const std::byte> nextMessage() const { return nextMessage_; };
  bool hasMessage() const { return !nextMessage_.empty(); }

  // seek the source so that after the call nextTime() is no earlier than given time
  virtual Timestamp seek(Timestamp time) = 0;

  // advance the source to the next message, return the time of the next message
  // if there is no event left, return Timestamp::max() and nextMessage() should return empty
  virtual Timestamp advance() = 0;

protected:
  // derived classes should populate these in their seek/advance overrides
  Timestamp nextTime_;
  std::span<const std::byte> nextMessage_;
};

} // namespace datasource
} // namespace bookproj