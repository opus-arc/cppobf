extern "C" const unsigned char ExternalData[] = {7};
class ExternalClass {
 public:
    int value() const;
};
int ExternalClass::value() const { return 7; }
