#include "itch50HistDataSource.h"
#include "itch50RawParser.h"
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace bookproj;

struct RawPrinter {
  template <typename Msg> void process(const Msg &msg) {
    std::cout << msg.toString() << std::endl;
  }
};

using bookproj::datasource::Itch50HistDataSource;
int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <itch50_dir> <date>" << std::endl;
    return 1;
  }

  Itch50HistDataSource::setRootPath(argv[1]);
  int date = std::stoi(argv[2]);
  std::unique_ptr<Itch50HistDataSource> source;
  try {
    source.reset(new Itch50HistDataSource(date));
  } catch (const std::runtime_error &e) {
    std::cerr << "Error creating data source: " << e.what() << std::endl;
    return 1;
  }

  RawPrinter handler;
  while (source->hasMessage()) {
    auto result = bookproj::itch50::parseMessage(source->nextMessage(), handler);
    if (result != bookproj::itch50::ParseResultType::Success) [[unlikely]] {
      std::cerr << "Error parsing message: " << bookproj::itch50::toString(result)
                << " file offset: " << source->currentOffset() << " time: "
                << std::format(
                       "{:%Y%m%d %H:%M:%S}",
                       std::chrono::locate_zone("America/New_York")->to_local(source->nextTime()))
                << std::endl;
      break;
    }
    source->advance();
  }
}