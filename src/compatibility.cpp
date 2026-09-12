#include "compatibility.h"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace cppobf {
namespace {

enum class State { kNormal, kString, kCharacter, kLineComment, kBlockComment };

bool IsIdentifierCharacter(char character) {
  const unsigned char value = static_cast<unsigned char>(character);
  return std::isalnum(value) != 0 || character == '_';
}

std::string EncodeNarrowString(std::string_view content, std::string_view prefix) {
  std::ostringstream encoded;
  encoded << prefix << '"';
  std::size_t bytes_in_chunk = 0;
  for (const unsigned char character : content) {
    if (bytes_in_chunk == 32) {
      encoded << "\" " << prefix << '"';
      bytes_in_chunk = 0;
    }
    switch (character) {
      case '\\': encoded << "\\\\"; break;
      case '"': encoded << "\\\""; break;
      case '\n': encoded << "\\n"; break;
      case '\r': encoded << "\\r"; break;
      case '\t': encoded << "\\t"; break;
      case '\v': encoded << "\\v"; break;
      case '\f': encoded << "\\f"; break;
      case '\b': encoded << "\\b"; break;
      case '?': encoded << "\\?"; break;
      default:
        if (character >= 0x20 && character <= 0x7e) {
          encoded << static_cast<char>(character);
        } else {
          encoded << '\\' << std::oct << std::setw(3) << std::setfill('0')
                  << static_cast<unsigned int>(character) << std::dec;
        }
    }
    ++bytes_in_chunk;
  }
  encoded << '"';
  return encoded.str();
}

struct RawPrefix {
  std::size_t length = 0;
  std::string_view literal_prefix;
  bool unsupported_wide = false;
};

RawPrefix MatchRawPrefix(std::string_view source, std::size_t position) {
  if (source.substr(position, 4) == "u8R\"") return {4, "u8", false};
  if (source.substr(position, 3) == "uR\"") return {3, "u", true};
  if (source.substr(position, 3) == "UR\"") return {3, "U", true};
  if (source.substr(position, 3) == "LR\"") return {3, "L", true};
  if (source.substr(position, 2) == "R\"") return {2, "", false};
  return {};
}

bool ValidDelimiter(std::string_view delimiter) {
  if (delimiter.size() > 16) return false;
  for (const unsigned char character : delimiter) {
    if (character <= 0x20 || character == '(' || character == ')' ||
        character == '\\' || character == 0x7f) {
      return false;
    }
  }
  return true;
}

}  // namespace

CompatibilityResult MakeCobfCompatible(std::string_view source) {
  CompatibilityResult result;
  result.source.reserve(source.size());
  State state = State::kNormal;

  for (std::size_t index = 0; index < source.size();) {
    const char current = source[index];
    const char next = index + 1 < source.size() ? source[index + 1] : '\0';

    if (state == State::kNormal) {
      const bool token_boundary =
          index == 0 || !IsIdentifierCharacter(source[index - 1]);
      const RawPrefix raw =
          token_boundary ? MatchRawPrefix(source, index) : RawPrefix{};
      if (raw.length != 0) {
        const std::size_t delimiter_start = index + raw.length;
        const std::size_t open = source.find('(', delimiter_start);
        if (open == std::string_view::npos ||
            !ValidDelimiter(source.substr(delimiter_start,
                                          open - delimiter_start))) {
          throw std::runtime_error("Malformed raw string literal near byte " +
                                   std::to_string(index) + ".");
        }
        if (raw.unsupported_wide) {
          throw std::runtime_error(
              "Wide raw string literals (uR/UR/LR) cannot be converted "
              "without a source-encoding assumption; aborting safely near byte " +
              std::to_string(index) + ".");
        }
        const std::string delimiter(source.substr(delimiter_start,
                                                  open - delimiter_start));
        const std::string closing = ")" + delimiter + "\"";
        const std::size_t close = source.find(closing, open + 1);
        if (close == std::string_view::npos) {
          throw std::runtime_error("Unterminated raw string literal near byte " +
                                   std::to_string(index) + ".");
        }
        result.source += EncodeNarrowString(
            source.substr(open + 1, close - open - 1), raw.literal_prefix);
        ++result.converted_raw_strings;
        index = close + closing.size();
        continue;
      }

      if (current == '/' && next == '/') {
        result.source += "//";
        index += 2;
        state = State::kLineComment;
        continue;
      }
      if (current == '/' && next == '*') {
        result.source += "/*";
        index += 2;
        state = State::kBlockComment;
        continue;
      }
      if (current == '"') {
        result.source += current;
        ++index;
        state = State::kString;
        continue;
      }
      if (current == '\'') {
        const bool digit_separator =
            index > 0 && index + 1 < source.size() &&
            std::isdigit(static_cast<unsigned char>(source[index - 1])) != 0 &&
            std::isdigit(static_cast<unsigned char>(source[index + 1])) != 0;
        if (digit_separator) {
          ++result.removed_digit_separators;
          ++index;
          continue;
        }
        result.source += current;
        ++index;
        state = State::kCharacter;
        continue;
      }
      result.source += current;
      ++index;
      continue;
    }

    result.source += current;
    ++index;
    if ((state == State::kString || state == State::kCharacter) &&
        current == '\\' && index < source.size()) {
      result.source += source[index++];
      continue;
    }
    if (state == State::kString && current == '"') state = State::kNormal;
    if (state == State::kCharacter && current == '\'') state = State::kNormal;
    if (state == State::kLineComment && current == '\n') state = State::kNormal;
    if (state == State::kBlockComment && current == '*' && next == '/') {
      result.source += '/';
      ++index;
      state = State::kNormal;
    }
  }

  if (state == State::kString || state == State::kCharacter ||
      state == State::kBlockComment) {
    throw std::runtime_error("Input ends inside a literal or block comment.");
  }
  return result;
}

}  // namespace cppobf
