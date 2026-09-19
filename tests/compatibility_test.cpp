#include "compatibility.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void Expect(std::string_view name, std::string_view input,
            std::string_view expected, std::size_t removed = 1) {
  const cppobf::CompatibilityResult result =
      cppobf::MakeCobfCompatible(input, true);
  if (result.source != expected ||
      result.removed_static_assert_messages != removed) {
    std::cerr << "FAILED: " << name << "\nexpected: " << expected
              << "\nactual:   " << result.source << "\nremoved:  "
              << result.removed_static_assert_messages << '\n';
    std::exit(EXIT_FAILURE);
  }
}

}  // namespace

int main() {
  Expect("simple", R"(static_assert(N == 88, "count must be 88");)",
         "static_assert(N == 88);");
  Expect("template comma",
         R"(static_assert(std::is_same_v<T, U>, "types differ");)",
         "static_assert(std::is_same_v<T, U>);");
  Expect("template call and function call",
         R"(static_assert(foo<A, B>() && bar(), "complex condition");)",
         "static_assert(foo<A, B>() && bar());");
  Expect("nested delimiters and comma operator",
         R"(static_assert((pred<A, B>(x, y) && values[choose(1, 2)] && Widget{1, 2}.ok()), "nested");)",
         "static_assert((pred<A, B>(x, y) && values[choose(1, 2)] && Widget{1, 2}.ok()));");
  Expect("commas and parentheses in diagnostic",
         R"cpp(static_assert(check<T, U>(), "expected (x, y)");)cpp",
         "static_assert(check<T, U>());");
  Expect("comments and adjacent literals",
         "static_assert(/* before */ trait<A, B> /* after */,\n"
         "              \"types \" /* join */ \"differ\");",
         "static_assert(/* before */ trait<A, B> /* after */);");
  Expect("preceded by digit separator",
         "constexpr auto mask = 0x8000'0000U;\n"
         "static_assert(N == 88, \"count must be 88\");",
         "constexpr auto mask = 0x80000000U;\nstatic_assert(N == 88);");
  Expect("single argument unchanged", "static_assert(ready<T, U>());",
         "static_assert(ready<T, U>());", 0);
  Expect("non-code occurrences unchanged",
         "// static_assert(false, \"comment\");\n"
         "const char* text = \"static_assert(false, \\\"text\\\")\";",
         "// static_assert(false, \"comment\");\n"
         "const char* text = \"static_assert(false, \\\"text\\\")\";",
         0);

  const std::string legacy = "static_assert(N == 88, \"count must be 88\");";
  const cppobf::CompatibilityResult cxx14 =
      cppobf::MakeCobfCompatible(legacy, false);
  if (cxx14.source != legacy || cxx14.removed_static_assert_messages != 0) {
    std::cerr << "FAILED: C++11/14 compatibility gate\n";
    return EXIT_FAILURE;
  }

  std::cout << "compatibility regression tests passed\n";
  return EXIT_SUCCESS;
}
