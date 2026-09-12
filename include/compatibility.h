#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace cppobf {

struct CompatibilityResult {
  std::string source;
  std::size_t removed_digit_separators = 0;
  std::size_t converted_raw_strings = 0;
};

CompatibilityResult MakeCobfCompatible(std::string_view source);

}  // namespace cppobf
