#include "HistDataSource.h"
#include "HistDataSourceFactory.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdio>

using namespace std::chrono_literals;

class TestDataSource : public bookproj::datasource::HistDataSource {
public:
  TestDataSource(int date) : value_(date) {}
  ~TestDataSource() override = default;

  using Timestamp = bookproj::datasource::HistDataSource::Timestamp;

  Timestamp advance() override {
    nextTime_ += 1s;
    value_ += 1;
    updateMsg();
    return nextTime_;
  }

  Timestamp seek(Timestamp time) override {
    nextTime_ = time;
    updateMsg();
    return time;
  }

private:
  void updateMsg() {
    auto len = snprintf(reinterpret_cast<char *>(msg_.data()), msg_.size(), "%d", value_);
    nextMessage_ = std::span<std::byte>(msg_.data(), len);
  }

  int value_;
  std::array<std::byte, 16> msg_ = {};
};

TEST_CASE("basic") {
  using bookproj::datasource::HistDataSource;
  using bookproj::datasource::HistDataSourceFactory;
  HistDataSourceFactory &factory = HistDataSourceFactory::instance();
  CHECK(factory.creators().empty());

  auto creator = [](int date) -> std::unique_ptr<bookproj::datasource::HistDataSource> {
    return std::make_unique<TestDataSource>(date);
  };
  CHECK(factory.registerCreator("test", creator));
  CHECK(factory.creators() == std::vector<std::string>{"test"});

  auto ds = factory.create("test", 20210101);
  CHECK(ds != nullptr);
  CHECK(ds->nextTime() == HistDataSource::Timestamp{});
  CHECK(ds->nextMessage().empty());

  HistDataSource::Timestamp start(100s);
  CHECK(ds->seek(HistDataSource::Timestamp(start)) == start);
  CHECK(ds->nextTime() == start);
  REQUIRE(!ds->nextMessage().empty());
  CHECK(strcmp(reinterpret_cast<const char *>(ds->nextMessage().data()), "20210101") == 0);

  CHECK(ds->advance() == start + 1s);
  CHECK(ds->nextTime() == start + 1s);
  REQUIRE(!ds->nextMessage().empty());
  CHECK(strcmp(reinterpret_cast<const char *>(ds->nextMessage().data()), "20210102") == 0);
}