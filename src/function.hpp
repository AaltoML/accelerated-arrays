#pragma once

#include <functional>
#include <future>
#include <array>

namespace accelerated {
struct Image;

namespace operations {
typedef std::function< std::future<void>(Image** inputs, int nInputs, Image &output) > Function;
typedef std::function< std::future<void>(Image &input, Image &output) > Unary;
typedef std::function< std::future<void>(Image &a, Image &b, Image &output) > Binary;

Function convert(const Unary &f);
Function convert(const Binary &f);

std::future<void> callUnary(Function &f, Image &input, Image &output);
std::future<void> callBinary(Function &f, Image &a, Image &b, Image &output);

template <std::size_t N> std::future<void> call(Function &f, std::array<Image*, N> &inputs, Image &output) {
    return f(reinterpret_cast<Image**>(&inputs), N, output);
}
}
}
