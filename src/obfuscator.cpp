#include "obfuscator.h"

#include "compatibility.h"
#include "process_runner.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mach-o/dyld.h>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace cppobf {
namespace fs = std::filesystem;
namespace {

constexpr std::size_t kMaximumVerificationIterations = 12;

std::string ReadFile(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot open file: " + path.string());
  std::ostringstream contents;
  contents << input.rdbuf();
  if (!input.good() && !input.eof()) {
    throw std::runtime_error("Cannot read file: " + path.string());
  }
  return contents.str();
}

void WriteFile(const fs::path& path, std::string_view contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) throw std::runtime_error("Cannot write file: " + path.string());
  output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  if (!output) throw std::runtime_error("Cannot write file: " + path.string());
}

std::set<std::string> ReadTokens(const fs::path& path) {
  std::set<std::string> tokens;
  std::istringstream lines(ReadFile(path));
  std::string line;
  while (std::getline(lines, line)) {
    const std::size_t first = line.find_first_not_of(" \t\r");
    if (first == std::string::npos || line[first] == '#') continue;
    const std::size_t last = line.find_last_not_of(" \t\r");
    tokens.insert(line.substr(first, last - first + 1));
  }
  return tokens;
}

std::set<std::string> CLinkageSymbols(std::string_view source) {
  // A C ABI declaration in an amalgamated input is an externally visible
  // contract. Preserve its name even when its definition appears elsewhere.
  const std::string text(source);
  static const std::regex linkage(R"re(\bextern\s*"C"\s*)re");
  static const std::regex function(R"(\b([A-Za-z_][A-Za-z_0-9]*)\s*\()");
  static const std::regex declaration(
      R"(\b([A-Za-z_][A-Za-z_0-9]*)\s*\([^;{}]*\)\s*;)");
  std::set<std::string> symbols;
  for (std::sregex_iterator match(text.begin(), text.end(), linkage), end;
       match != end; ++match) {
    const std::size_t after = static_cast<std::size_t>(match->position() +
                                                        match->length());
    if (after < text.size() && text[after] == '{') {
      const std::size_t close = text.find('}', after + 1);
      if (close == std::string::npos) continue;
      const std::string block = text.substr(after + 1, close - after - 1);
      for (std::sregex_iterator declaration_match(
               block.begin(), block.end(), declaration), declaration_end;
           declaration_match != declaration_end; ++declaration_match) {
        symbols.insert((*declaration_match)[1].str());
      }
    } else {
      const std::size_t semicolon = text.find(';', after);
      const std::size_t brace = text.find('{', after);
      const std::size_t stop = std::min(semicolon, brace);
      if (stop == std::string::npos) continue;
      const std::string declaration_text = text.substr(after, stop - after);
      std::smatch function_match;
      if (std::regex_search(declaration_text, function_match, function)) {
        symbols.insert(function_match[1].str());
      }
    }
  }
  return symbols;
}

void WriteTokens(const fs::path& path, const std::set<std::string>& tokens) {
  std::ostringstream output;
  for (const std::string& token : tokens) output << token << '\n';
  WriteFile(path, output.str());
}

class TemporaryDirectory {
 public:
  TemporaryDirectory(const fs::path& parent, bool keep) : keep_(keep) {
    fs::create_directories(parent);
    std::string pattern = (parent / ".cppobf-temp-XXXXXX").string();
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');
    char* created = mkdtemp(writable.data());
    if (created == nullptr) {
      throw std::runtime_error("Cannot create temporary directory: " +
                               std::string(std::strerror(errno)));
    }
    path_ = created;
  }
  ~TemporaryDirectory() {
    if (!keep_) {
      std::error_code ignored;
      fs::remove_all(path_, ignored);
    }
  }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
  bool keep_;
};

std::unordered_map<std::string, std::string> ReadMap(const fs::path& path) {
  std::unordered_map<std::string, std::string> by_obfuscated;
  std::istringstream lines(ReadFile(path));
  std::string original;
  std::string obfuscated;
  while (lines >> original >> obfuscated) {
    by_obfuscated[obfuscated] = original;
  }
  return by_obfuscated;
}

bool StartsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.substr(0, prefix.size()) == prefix;
}

bool ClearlyExternal(std::string_view name, std::string_view diagnostic) {
  static const std::set<std::string> keywords = {
      "alignas", "alignof", "char8_t", "char16_t", "char32_t",
      "concept", "consteval", "constexpr", "constinit", "co_await",
      "co_return", "co_yield", "decltype", "final", "noexcept",
      "nullptr", "override", "requires", "static_assert", "thread_local"};
  if (keywords.contains(std::string(name))) return true;

  // COBF also renames standard attribute names. Clang normally warns and
  // ignores an unknown attribute, so make that warning an error below and
  // preserve only a recognized standard attribute from the identifier map.
  static const std::set<std::string> standard_attributes = {
      "assume", "carries_dependency", "deprecated", "fallthrough",
      "likely", "maybe_unused", "nodiscard", "no_unique_address",
      "noreturn", "unlikely"};
  if (diagnostic.find("unknown attribute") != std::string_view::npos &&
      standard_attributes.contains(std::string(name))) {
    return true;
  }

  if (diagnostic.find("namespace 'std") != std::string_view::npos ||
      diagnostic.find("in 'std::") != std::string_view::npos) {
    return true;
  }
  static constexpr std::string_view system_contexts[] = {
      "AudioBuffer", "AudioBufferList", "AudioComponentDescription",
      "AudioStreamBasicDescription", "AURenderCallbackStruct", "MIDIPacket",
      "MIDIPacketList", "termios", "kevent"};
  for (const std::string_view context : system_contexts) {
    if (diagnostic.find(context) != std::string_view::npos) return true;
  }

  static const std::set<std::string> exact = {
      "CFSTR", "EXIT_FAILURE", "EXIT_SUCCESS", "STDIN_FILENO", "errno",
      "getopt_long", "isatty", "kqueue", "main", "noErr", "optarg",
      "pthread_sigmask", "read", "sigaddset", "sigemptyset", "sigwait",
      "tcgetattr", "tcsetattr", "write"};
  if (exact.contains(std::string(name))) return true;
  if (StartsWith(name, "kAudio") || StartsWith(name, "MIDI") ||
      StartsWith(name, "AudioUnit") || StartsWith(name, "AudioComponent") ||
      StartsWith(name, "EV_") || StartsWith(name, "EVFILT_") ||
      StartsWith(name, "SIG") || StartsWith(name, "CLOCK_") ||
      StartsWith(name, "pthread_")) {
    return true;
  }
  return false;
}

std::set<std::string> RepairCandidates(
    std::string_view diagnostics,
    const std::unordered_map<std::string, std::string>& map,
    const std::set<std::string>& already_preserved) {
  std::set<std::string> candidates;
  const std::regex obfuscated(R"(\bl[0-9A-Za-z]+\b)");
  std::istringstream lines{std::string(diagnostics)};
  std::string line;
  while (std::getline(lines, line)) {
    if (line.find(" error:") == std::string::npos) continue;
    for (std::sregex_iterator match(line.begin(), line.end(), obfuscated), end;
         match != end; ++match) {
      const auto mapped = map.find(match->str());
      if (mapped != map.end() &&
          !already_preserved.contains(mapped->second) &&
          ClearlyExternal(mapped->second, line)) {
        candidates.insert(mapped->second);
      }
    }
  }
  return candidates;
}

std::string Trim(std::string line) {
  const std::size_t first = line.find_first_not_of(" \t\r");
  if (first == std::string::npos) return {};
  const std::size_t last = line.find_last_not_of(" \t\r");
  return line.substr(first, last - first + 1);
}

bool SupportsSingleArgumentStaticAssert(std::string_view standard) {
  return standard == "c++17" || standard == "c++20" ||
         standard == "c++23" || standard == "c++2b" ||
         standard == "c++26";
}

std::string InlineCobfHeaders(std::string_view source,
                              const fs::path& directory) {
  std::istringstream lines{std::string(source)};
  std::ostringstream output;
  std::string line;
  while (std::getline(lines, line)) {
    const std::string trimmed = Trim(line);
    std::string header_name;
    if (trimmed == "#include\"cobf.h\"" ||
        trimmed == "#include \"cobf.h\"") {
      header_name = "cobf.h";
    } else if (trimmed == "#include\"uncobf.h\"" ||
               trimmed == "#include \"uncobf.h\"") {
      header_name = "uncobf.h";
    }
    if (header_name.empty()) {
      output << line << '\n';
      continue;
    }
    const fs::path header = directory / header_name;
    if (!fs::exists(header)) {
      throw std::runtime_error("COBF output references a missing header: " +
                               header.string());
    }
    output << ReadFile(header);
    if (!output.str().ends_with('\n')) output << '\n';
  }
  const std::string merged = output.str();
  if (merged.find("#include\"cobf.h\"") != std::string::npos ||
      merged.find("#include \"cobf.h\"") != std::string::npos ||
      merged.find("#include\"uncobf.h\"") != std::string::npos ||
      merged.find("#include \"uncobf.h\"") != std::string::npos) {
    throw std::runtime_error("Could not safely inline a COBF support header.");
  }
  return merged;
}

std::string RestoreLanguageLinkage(std::string_view source) {
  // COBF encodes ordinary strings as hex escapes and may insert an empty
  // adjacent literal. In an extern language-linkage declaration, C++ requires
  // the single literal "C"; even equivalent escapes are invalid there.
  static const std::regex c_linkage(
      R"re(\bextern\s*(?:""\s*)?"\\x43")re");
  return std::regex_replace(std::string(source), c_linkage, "extern \"C\" ");
}

void PublishAtomically(const fs::path& output, std::string_view contents) {
  fs::create_directories(output.parent_path());
  const fs::path temporary = output.string() + ".cppobf-new";
  WriteFile(temporary, contents);
  std::error_code error;
  fs::rename(temporary, output, error);
  if (error) {
    fs::remove(temporary);
    throw std::runtime_error("Cannot publish output: " + error.message());
  }
}

}  // namespace

fs::path ExecutableDirectory() {
  std::uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> buffer(size + 1);
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
    throw std::runtime_error("Cannot locate the cppobf executable.");
  }
  return fs::weakly_canonical(fs::path(buffer.data())).parent_path();
}

Obfuscator::Obfuscator(fs::path installation_root)
    : installation_root_(std::move(installation_root)) {}

ObfuscationReport Obfuscator::Run(const ObfuscationOptions& options) const {
  if (!fs::is_regular_file(options.input)) {
    throw std::runtime_error("Input is not a regular file: " +
                             options.input.string());
  }
  if (!std::regex_match(options.standard,
                        std::regex(R"(c\+\+(11|14|17|20|23|2b|26))"))) {
    throw std::runtime_error("Unsupported --std value: " + options.standard);
  }

  const fs::path root = fs::weakly_canonical(installation_root_);
  const fs::path cobf = root / "vendor/cobf/bin/cobf";
  const fs::path wrapper = root / "vendor/cobf/etc/pp_clang_cpp";
  const fs::path make_tokens = root / "vendor/cobf/etc/cc.mak";
  const fs::path cpp_tokens = root / "vendor/cobf/etc/cpp.tok";
  const fs::path library_tokens = root / "vendor/cobf/etc/cpplib.tok";
  const fs::path preset = root / "presets/default_external.tok";
  for (const fs::path& resource :
       {cobf, wrapper, make_tokens, cpp_tokens, library_tokens, preset}) {
    if (!fs::exists(resource)) {
      throw std::runtime_error("Missing cppobf resource: " + resource.string());
    }
  }

  const fs::path absolute_output = fs::absolute(options.output);
  // COBF 1.06 constructs its preprocessor command internally without shell
  // quoting. Keep every path that COBF passes to that command free of spaces.
  TemporaryDirectory temporary(fs::temp_directory_path(), options.keep_temp);
  const fs::path local_wrapper = temporary.path() / "pp_clang_cpp";
  WriteFile(local_wrapper,
            "#!/bin/sh\nset -eu\necho \"Preprocessing $1\"\n"
            "exec xcrun clang++ -std=" +
                options.standard + " -E -P \"$1\" > \"$2\"\n");
  fs::permissions(local_wrapper,
                  fs::perms::owner_exec | fs::perms::owner_read |
                      fs::perms::owner_write | fs::perms::group_exec |
                      fs::perms::group_read | fs::perms::others_exec |
                      fs::perms::others_read,
                  fs::perm_options::replace);
  const fs::path compatible_input = temporary.path() / "input.cpp";
  const CompatibilityResult compatibility =
      MakeCobfCompatible(ReadFile(options.input),
                         SupportsSingleArgumentStaticAssert(options.standard));
  WriteFile(compatible_input, compatibility.source);

  std::set<std::string> external_tokens = ReadTokens(preset);
  const std::set<std::string> c_symbols =
      CLinkageSymbols(compatibility.source);
  external_tokens.insert(c_symbols.begin(), c_symbols.end());
  ObfuscationReport report;
  report.removed_digit_separators = compatibility.removed_digit_separators;
  report.converted_raw_strings = compatibility.converted_raw_strings;
  report.removed_static_assert_messages =
      compatibility.removed_static_assert_messages;

  for (std::size_t iteration = 1;
       iteration <= kMaximumVerificationIterations; ++iteration) {
    report.verification_iterations = iteration;
    const fs::path round =
        temporary.path() / ("round-" + std::to_string(iteration));
    const fs::path generated = round / "output/input.cpp";
    const fs::path map_path = round / "map.txt";
    const fs::path token_path = round / "external.tok";
    fs::create_directories(round / "output");
    WriteTokens(token_path, external_tokens);

    const ProcessResult cobf_result = ProcessRunner::Run({
        cobf.string(), "-p", local_wrapper.string(), "-m", make_tokens.string(),
        "-t", cpp_tokens.string(), "-t", library_tokens.string(), "-u",
        "-t", token_path.string(), "-dm", map_path.string(), "-o",
        (round / "output").string(), "-b", compatible_input.string()});
    WriteFile(round / "cobf-output.txt", cobf_result.output);
    if (cobf_result.exit_code != 0 || !fs::exists(generated) ||
        !fs::exists(map_path)) {
      throw std::runtime_error("COBF failed (exit " +
                               std::to_string(cobf_result.exit_code) + "):\n" +
                               cobf_result.output);
    }

    std::string candidate = RestoreLanguageLinkage(
        InlineCobfHeaders(ReadFile(generated), generated.parent_path()));
    const fs::path candidate_path = round / "candidate.cpp";
    WriteFile(candidate_path, candidate);
    const auto map = ReadMap(map_path);
    report.mapped_identifiers = map.size();

    if (!options.verify) {
      PublishAtomically(absolute_output, candidate);
      report.preserved_identifiers = external_tokens.size();
      report.temp_directory =
          options.keep_temp ? temporary.path() : fs::path{};
      return report;
    }

    const ProcessResult verification = ProcessRunner::Run(
        {"xcrun", "clang++", "-std=" + options.standard, "-fsyntax-only",
         "-Werror=unknown-attributes", candidate_path.string()});
    WriteFile(round / "clang-diagnostics.txt", verification.output);
    if (verification.exit_code == 0) {
      PublishAtomically(absolute_output, candidate);
      report.preserved_identifiers = external_tokens.size();
      report.temp_directory =
          options.keep_temp ? temporary.path() : fs::path{};
      return report;
    }

    const std::set<std::string> repairs =
        RepairCandidates(verification.output, map, external_tokens);
    if (repairs.empty()) {
      throw std::runtime_error(
          "Verification failed, but no clearly external identifier could be "
          "preserved safely. User identifiers were not added automatically.\n" +
          verification.output);
    }
    external_tokens.insert(repairs.begin(), repairs.end());
  }

  throw std::runtime_error("Verification did not converge after " +
                           std::to_string(kMaximumVerificationIterations) +
                           " iterations.");
}

}  // namespace cppobf
