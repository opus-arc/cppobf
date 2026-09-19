#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace cppobf {

struct ObfuscationOptions {
  std::filesystem::path input;
  std::filesystem::path output;
  std::string standard = "c++20";
  bool verify = false;
  bool keep_temp = false;
};

struct ObfuscationReport {
  std::size_t mapped_identifiers = 0;
  std::size_t preserved_identifiers = 0;
  std::size_t verification_iterations = 0;
  std::size_t removed_digit_separators = 0;
  std::size_t converted_raw_strings = 0;
  std::size_t removed_static_assert_messages = 0;
  std::filesystem::path temp_directory;
};

class Obfuscator {
 public:
  explicit Obfuscator(std::filesystem::path installation_root);
  ObfuscationReport Run(const ObfuscationOptions& options) const;

 private:
  std::filesystem::path installation_root_;
};

std::filesystem::path ExecutableDirectory();

}  // namespace cppobf
