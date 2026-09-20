#include <cstdio>

constexpr int Choose(int left, int right) { return left < right ? right : left; }
static_assert(Choose(1, 2) == 2, "nested call, comma");
static_assert(1'000'000 == 1000000, "digit separator");

int main() {
  const char* text = R"tag(alpha
beta)tag";
  std::printf("%d|%d|%s\n", Choose(1, 2), 1'000'000, text);
}
