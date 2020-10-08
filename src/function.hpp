#pragma once

#include <functional>
#include <array>

#include "future.hpp"

namespace accelerated {
struct Image;
struct Future;

namespace operations {
typedef std::function< Future(Image** inputs, int nInputs, Image &output) > Function;
typedef std::function< Future(Image &output) > Nullary;
typedef std::function< Future(Image &input, Image &output) > Unary;
typedef std::function< Future(Image &a, Image &b, Image &output) > Binary;

Function convert(const Nullary &f);
Function convert(const Unary &f);
Function convert(const Binary &f);

Future callNullary(Function &f, Image &output);
Future callUnary(Function &f, Image &input, Image &output);
Future callBinary(Function &f, Image &a, Image &b, Image &output);

template <std::size_t N> Future call(Function &f, std::array<Image*, N> &inputs, Image &output) {
    return f(reinterpret_cast<Image**>(&inputs), N, output);
}
}
}
