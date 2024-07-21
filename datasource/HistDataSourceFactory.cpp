#include "HistDataSourceFactory.h"

namespace bookproj {
namespace datasource {

HistDataSourceFactory &HistDataSourceFactory::instance() {
  static HistDataSourceFactory factory;
  return factory;
}

bool HistDataSourceFactory::registerCreator(std::string name, CreatorCB creator) {
  return creators_.emplace(std::move(name), std::move(creator)).second;
}

std::unique_ptr<HistDataSource> HistDataSourceFactory::create(std::string name, int date) {
  auto it = creators_.find(name);
  if (it == creators_.end()) {
    return nullptr;
  }
  return it->second(date);
}

std::vector<std::string> HistDataSourceFactory::creators() const {
  std::vector<std::string> result;
  for (const auto &pair : creators_) {
    result.push_back(pair.first);
  }
  return result;
}

} // namespace datasource
} // namespace bookproj