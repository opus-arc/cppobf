#include "obfuscator.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr const char* kVersion = "cppobf 0.1.0";

void PrintHelp() {
  std::cout << R"(cppobf — C++ source obfuscation frontend for COBF

Usage:
  cppobf <input.cpp> -o <output.cpp> [options]

Options:
  -o, --output <file>   Output obfuscated C++ source
      --verify          Verify output with Apple Clang
      --std <standard>  C++ standard used for verification
                        Default: c++20
      --keep-temp       Keep intermediate COBF files
  -h, --help            Show this help
  -v, --version         Show version

Typical workflow:
  cpp-amalgamate amalgamation.cpp > single.cpp
  cppobf single.cpp -o obfuscated.cpp --verify
)";
}

cppobf::ObfuscationOptions ParseArguments(int argc, char* argv[]) {
  cppobf::ObfuscationOptions options;
  bool have_input = false;
  bool have_output = false;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--verify") {
      options.verify = true;
    } else if (argument == "--keep-temp") {
      options.keep_temp = true;
    } else if (argument == "-o" || argument == "--output") {
      if (++index >= argc) throw std::runtime_error(argument + " needs a file.");
      options.output = argv[index];
      have_output = true;
    } else if (argument == "--std") {
      if (++index >= argc) throw std::runtime_error("--std needs a value.");
      options.standard = argv[index];
    } else if (!argument.empty() && argument.front() == '-') {
      throw std::runtime_error("Unknown option: " + argument);
    } else if (!have_input) {
      options.input = argument;
      have_input = true;
    } else {
      throw std::runtime_error("Only one input .cpp is supported.");
    }
  }
  if (!have_input) throw std::runtime_error("Missing input .cpp file.");
  if (!have_output) throw std::runtime_error("Missing -o/--output.");
  if (options.input.extension() != ".cpp") {
    throw std::runtime_error("Input must have a .cpp extension.");
  }
  if (options.output.extension() != ".cpp") {
    throw std::runtime_error("Output must have a .cpp extension.");
  }
  return options;
}

}  // namespace

int main(int argc, char* argv[]) {
  try {
    if (argc == 2) {
      const std::string argument = argv[1];
      if (argument == "-h" || argument == "--help") {
        PrintHelp();
        return 0;
      }
      if (argument == "-v" || argument == "--version") {
        std::cout << kVersion << '\n';
        return 0;
      }
    }

    const cppobf::ObfuscationOptions options = ParseArguments(argc, argv);
    const cppobf::ObfuscationReport report =
        cppobf::Obfuscator(cppobf::ExecutableDirectory()).Run(options);
    std::cout << "Obfuscated: " << std::filesystem::absolute(options.output)
              << '\n'
              << "Mapped identifiers: " << report.mapped_identifiers << '\n'
              << "Preserved external identifiers: "
              << report.preserved_identifiers << '\n'
              << "Compatibility: removed "
              << report.removed_digit_separators << " digit separators, converted "
              << report.converted_raw_strings << " raw strings\n";
    if (options.verify) {
      std::cout << "Verification: passed in " << report.verification_iterations
                << " iteration(s)\n";
    }
    if (options.keep_temp) {
      std::cout << "Intermediate files: " << report.temp_directory << '\n';
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "cppobf: " << error.what() << '\n';
    return 1;
  }
}
