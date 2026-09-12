#pragma once

#include <string>
#include <vector>

namespace cppobf {

struct ProcessResult {
  int exit_code = -1;
  std::string output;
};

class ProcessRunner {
 public:
  static ProcessResult Run(const std::vector<std::string>& arguments);
};

}  // namespace cppobf
