#include "itch50RawParser.h"

namespace bookproj {
namespace itch50 {

std::string toString(ParseResultType type) {
  switch (type) {
  case ParseResultType::Success:
    return "Success";
  case ParseResultType::BadMsgType:
    return "BadMsgType";
  case ParseResultType::BadSize:
    return "BadSize";
  }
  return "Unknown";
}

} // namespace itch50
} // namespace bookproj