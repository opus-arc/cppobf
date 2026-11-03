// Explicit C++/data ABI shared with another translation unit.
extern "C" const unsigned char ExternalData[];
class ExternalClass {
 public:
    int value() const;
};
int main() {
    const int local_result = ExternalClass{}.value();
    return local_result == ExternalData[0] ? 0 : 1;
}
