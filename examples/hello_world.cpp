#include <iostream>
#include <string>
#include <vector>
#include "absl/strings/str_join.h"

int main(int argc, char *argv[]) {
  std::vector<std::string> v = {"foo","bar","baz"};
  for (int ii = 1; ii < argc; ii++) {
    v.push_back(argv[ii]);
  }
  std::string s = absl::StrJoin(v, "-");

  std::cout << "Joined string: " << s << "\n";
}