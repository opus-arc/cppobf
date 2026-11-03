// AI-assisted regression: literal values, types and macro spelling must survive.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <type_traits>

#define SPELL(value) #value
#define PASTE(a, b) a##b
#define HEX_VALUE 0x1.abcdef0123456p-12
constexpr double values[] = {
    HEX_VALUE, 0X1.Ap+2, 0x1p0, 0x.8p1, 0x1.p-1, -0x0p0,
    0x1.0000000000001p0, 0x0.0000000000001p-1022,
    0x1.fffffffffffffp+1023, 0x0p0, 0x1p0+0x1p1,
    0xA'B.C'Dp-4, PASTE(0x1.abcde, p3)
};
static_assert(std::is_same_v<decltype(0x1.8p0f), float>);
static_assert(std::is_same_v<decltype(0x1.8p0L), long double>);
static_assert(0b1010'1010ULL == 170ULL);
static_assert(0xAB'CDU == 43981U);
int main() {
    for (double value : values) {
        std::uint64_t bits;
        std::memcpy(&bits, &value, sizeof(value));
        std::printf("%016llx\n", static_cast<unsigned long long>(bits));
    }
    std::printf("%a %La %s %s\n", 0x1.23456p-4f, 0x1.8p0L,
                SPELL(0x1.abcdef0123456p-12), "0x1.abcdef0123456p-12");
}
