#include "../standard_ops.hpp"

namespace accelerated {
struct Processor;
namespace cpu {
class Image;
namespace operations {
typedef std::function< void(Image **inputs, int nInputs, Image &output) > NAry;
typedef std::function< void(Image &output) > Nullary;
typedef std::function< void(Image &input, Image &output) > Unary;
typedef std::function< void(Image &a, Image &b, Image &output) > Binary;

class Factory : public ::accelerated::operations::StandardFactory {
public:
    virtual ::accelerated::operations::Function wrapNAry(const NAry &func) = 0;
    template <class T> ::accelerated::operations::Function wrap(const T &func) {
        return wrapNAry(::accelerated::operations::sync::convert(func));
    }
};

// may confuse the compiler due to the inherited "create" methods if inside
// the above class and called just "create"
std::unique_ptr<Factory> createFactory(Processor &processor);
}
}
}
