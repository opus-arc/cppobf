#include "compatibility.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace cppobf {
namespace {

enum class State { kNormal, kString, kCharacter, kLineComment, kBlockComment };

bool IsIdentifierCharacter(char character) {
  const unsigned char value = static_cast<unsigned char>(character);
  return std::isalnum(value) != 0 || character == '_';
}

bool IsDigitSeparator(std::string_view source, std::size_t position) {
  return position > 0 && position + 1 < source.size() &&
         std::isdigit(static_cast<unsigned char>(source[position - 1])) != 0 &&
         std::isdigit(static_cast<unsigned char>(source[position + 1])) != 0;
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

std::optional<std::size_t> RawStringEnd(std::string_view source,
                                        std::size_t position) {
  const RawPrefix raw = MatchRawPrefix(source, position);
  if (raw.length == 0) return std::nullopt;
  const std::size_t delimiter_start = position + raw.length;
  const std::size_t open = source.find('(', delimiter_start);
  if (open == std::string_view::npos ||
      !ValidDelimiter(source.substr(delimiter_start,
                                    open - delimiter_start))) {
    return std::nullopt;
  }
  const std::string delimiter(source.substr(delimiter_start,
                                            open - delimiter_start));
  const std::string closing = ")" + delimiter + "\"";
  const std::size_t close = source.find(closing, open + 1);
  if (close == std::string_view::npos) return std::nullopt;
  return close + closing.size();
}

std::size_t SkipQuoted(std::string_view source, std::size_t position,
                       char quote) {
  ++position;
  while (position < source.size()) {
    if (source[position] == '\\' && position + 1 < source.size()) {
      position += 2;
    } else if (source[position++] == quote) {
      break;
    }
  }
  return position;
}

std::size_t SkipLineComment(std::string_view source, std::size_t position) {
  const std::size_t newline = source.find('\n', position + 2);
  return newline == std::string_view::npos ? source.size() : newline + 1;
}

std::size_t SkipBlockComment(std::string_view source, std::size_t position) {
  const std::size_t close = source.find("*/", position + 2);
  return close == std::string_view::npos ? source.size() : close + 2;
}

std::size_t SkipTrivia(std::string_view source, std::size_t position,
                       std::size_t end) {
  while (position < end) {
    if (std::isspace(static_cast<unsigned char>(source[position])) != 0) {
      ++position;
    } else if (position + 1 < end && source[position] == '/' &&
               source[position + 1] == '/') {
      position = std::min(SkipLineComment(source, position), end);
    } else if (position + 1 < end && source[position] == '/' &&
               source[position + 1] == '*') {
      position = std::min(SkipBlockComment(source, position), end);
    } else {
      break;
    }
  }
  return position;
}

std::optional<std::size_t> OrdinaryStringEnd(std::string_view source,
                                             std::size_t position,
                                             std::size_t end) {
  std::size_t quote = position;
  if (source.substr(position, 3) == "u8\"") {
    quote += 2;
  } else if (position + 1 < end &&
             (source[position] == 'u' || source[position] == 'U' ||
              source[position] == 'L') &&
             source[position + 1] == '"') {
    ++quote;
  }
  if (quote >= end || source[quote] != '"') return std::nullopt;
  const std::size_t after = SkipQuoted(source, quote, '"');
  if (after > end || after == 0 || source[after - 1] != '"') {
    return std::nullopt;
  }
  return after;
}

bool IsStringLiteralSequence(std::string_view source, std::size_t begin,
                             std::size_t end) {
  std::size_t position = SkipTrivia(source, begin, end);
  bool found = false;
  while (position < end) {
    std::optional<std::size_t> after;
    const bool token_boundary =
        position == 0 || !IsIdentifierCharacter(source[position - 1]);
    if (token_boundary) after = RawStringEnd(source, position);
    if (!after.has_value()) after = OrdinaryStringEnd(source, position, end);
    if (!after.has_value() || *after > end) return false;
    found = true;
    position = SkipTrivia(source, *after, end);
  }
  return found;
}

struct StaticAssertRange {
  std::size_t comma = 0;
  std::size_t close = 0;
};

std::optional<StaticAssertRange> FindStaticAssertDiagnostic(
    std::string_view source, std::size_t keyword) {
  constexpr std::string_view token = "static_assert";
  std::size_t position = SkipTrivia(source, keyword + token.size(),
                                    source.size());
  if (position == source.size() || source[position] != '(') {
    return std::nullopt;
  }

  int parentheses = 1;
  int brackets = 0;
  int braces = 0;
  std::optional<std::size_t> last_top_level_comma;
  ++position;
  while (position < source.size()) {
    const bool token_boundary =
        position == 0 || !IsIdentifierCharacter(source[position - 1]);
    if (token_boundary) {
      if (const auto raw_end = RawStringEnd(source, position)) {
        position = *raw_end;
        continue;
      }
    }
    const char current = source[position];
    const char next =
        position + 1 < source.size() ? source[position + 1] : '\0';
    if (current == '/' && next == '/') {
      position = SkipLineComment(source, position);
      continue;
    }
    if (current == '/' && next == '*') {
      position = SkipBlockComment(source, position);
      continue;
    }
    if (current == '\'' && IsDigitSeparator(source, position)) {
      ++position;
      continue;
    }
    if (current == '"' || current == '\'') {
      position = SkipQuoted(source, position, current);
      continue;
    }
    if (current == '(') {
      ++parentheses;
    } else if (current == ')') {
      --parentheses;
      if (parentheses == 0) {
        if (last_top_level_comma.has_value() &&
            IsStringLiteralSequence(source, *last_top_level_comma + 1,
                                    position)) {
          return StaticAssertRange{*last_top_level_comma, position};
        }
        return std::nullopt;
      }
    } else if (current == '[') {
      ++brackets;
    } else if (current == ']' && brackets > 0) {
      --brackets;
    } else if (current == '{') {
      ++braces;
    } else if (current == '}' && braces > 0) {
      --braces;
    } else if (current == ',' && parentheses == 1 && brackets == 0 &&
               braces == 0) {
      // The C++20 diagnostic is an unevaluated string literal and therefore
      // has no comma outside the literal. Selecting the last top-level comma
      // safely ignores commas in template argument lists without trying to
      // guess whether '<' and '>' are templates or comparison operators.
      last_top_level_comma = position;
    }
    ++position;
  }
  return std::nullopt;
}

std::string RemoveStaticAssertMessages(std::string_view source,
                                       std::size_t& removed) {
  constexpr std::string_view token = "static_assert";
  std::string output;
  output.reserve(source.size());
  std::size_t copied_through = 0;
  std::size_t position = 0;
  while (position < source.size()) {
    const bool token_boundary =
        position == 0 || !IsIdentifierCharacter(source[position - 1]);
    if (token_boundary) {
      if (const auto raw_end = RawStringEnd(source, position)) {
        position = *raw_end;
        continue;
      }
    }
    const char current = source[position];
    const char next =
        position + 1 < source.size() ? source[position + 1] : '\0';
    if (current == '/' && next == '/') {
      position = SkipLineComment(source, position);
      continue;
    }
    if (current == '/' && next == '*') {
      position = SkipBlockComment(source, position);
      continue;
    }
    if (current == '\'' && IsDigitSeparator(source, position)) {
      ++position;
      continue;
    }
    if (current == '"' || current == '\'') {
      position = SkipQuoted(source, position, current);
      continue;
    }
    if (token_boundary && source.substr(position, token.size()) == token &&
        (position + token.size() == source.size() ||
         !IsIdentifierCharacter(source[position + token.size()]))) {
      if (const auto range = FindStaticAssertDiagnostic(source, position)) {
        output.append(source.substr(copied_through,
                                    range->comma - copied_through));
        output.push_back(')');
        copied_through = range->close + 1;
        position = copied_through;
        ++removed;
        continue;
      }
    }
    ++position;
  }
  output.append(source.substr(copied_through));
  return output;
}

}  // namespace

CompatibilityResult MakeCobfCompatible(
    std::string_view source, bool supports_single_argument_static_assert) {
  CompatibilityResult result;
  const std::string static_assert_compatible =
      supports_single_argument_static_assert
          ? RemoveStaticAssertMessages(
                source, result.removed_static_assert_messages)
          : std::string(source);
  source = static_assert_compatible;
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
        if (IsDigitSeparator(source, index)) {
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
