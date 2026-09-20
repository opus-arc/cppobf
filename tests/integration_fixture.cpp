#include <cstdio>

constexpr int Choose(int left, int right) { return left < right ? right : left; }
static_assert(Choose(1, 2) == 2, "nested call, comma");
static_assert(1'000'000 == 1000000, "digit separator");

extern "C" int ForeignValue(int value) { return value + 1; }
extern "C" {
int ApiEntry(int value);
}
int ApiEntry(int value) { return value + 3; }
[[nodiscard]] int Doubled(int value) { return value * 2; }

int main() {
  const char* text = R"tag(alpha
beta)tag";
  std::printf("%d|%d|%s\n", Doubled(ApiEntry(ForeignValue(Choose(1, 2)))),
              1'000'000, text);
}
