#pragma once

#include "HistDataSource.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace bookproj {
namespace datasource {

class HistDataSourceFactory {
public:
  using CreatorCB = std::function<std::unique_ptr<HistDataSource>(int date)>;

  static HistDataSourceFactory &instance();

  bool registerCreator(std::string name, CreatorCB creator);

  std::unique_ptr<HistDataSource> create(std::string name, int date);

  std::vector<std::string> creators() const;

private:
  HistDataSourceFactory() = default;
  HistDataSourceFactory(const HistDataSourceFactory &) = delete;
  HistDataSourceFactory &operator=(const HistDataSourceFactory &) = delete;

  std::unordered_map<std::string, CreatorCB> creators_;
};

} // namespace datasource
} // namespace bookproj
