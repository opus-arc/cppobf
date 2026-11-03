// AI-assisted regression for Clang attributes, NEON and Accelerate names.
#include <Accelerate/Accelerate.h>
#include <arm_neon.h>
#include <cmath>
#include <iostream>
#include <streambuf>
typedef double Pair __attribute__((ext_vector_type(2)));
class MemoryBuffer : public std::streambuf {
 public:
    explicit MemoryBuffer(char* data) { setg(data, data, data + 1); }
};
int main() {
    Pair pair = {1.0, 2.0};
    double input[2] = {pair[0], pair[1]}, output[2] = {};
    auto v = vnegq_f64(vld1q_f64(input));
    vst1q_f64(output, v);
    const double matrix[4] = {1, 0, 0, 2};
    cblas_dgemv(CblasRowMajor, CblasNoTrans, 2, 2, 1.0, matrix, 2,
                input, 1, 0.0, output, 1);
    if (__builtin_available(macOS 13.3, *)) {
        BLASSetThreading(BLAS_THREADING_SINGLE_THREADED);
    }
    char data[] = "x";
    MemoryBuffer buffer(data);
    std::istream stream(&buffer);
    stream.setstate(std::ios::goodbit);
    std::cout << output[0] << ' ' << output[1] << ' ' << stream.get()
              << ' ' << std::isinf(INFINITY) << ' ' << std::isnan(NAN) << '\n';
}
