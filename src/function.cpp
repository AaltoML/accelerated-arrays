#include <cassert>

#include "function.hpp"
#include "image.hpp"

namespace accelerated {
namespace operations {

Function convert(const Unary &f) {
    return [f](Image** inputs, int nInputs, Image &output) -> Future {
        assert(nInputs == 1);
        return f(**inputs, output);
    };
}

Function convert(const Binary &f) {
    return [f](Image** inputs, int nInputs, Image &output) -> Future {
        assert(nInputs == 2);
        return f(*inputs[0], *inputs[1], output);
    };
}

Future callUnary(Function &f, Image &input, Image &output) {
    std::array<Image*, 1> arr = { &input };
    return call(f, arr, output);
}

Future callBinary(Function &f, Image &a, Image &b, Image &output) {
    std::array<Image*, 2> arr = { &a, &b };
    return call(f, arr, output);
}

}
}
