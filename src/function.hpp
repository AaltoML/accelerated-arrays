#pragma once

#include <functional>
#include <future>
#include <array>

namespace accelerated {
struct Image;
struct Future;

namespace operations {
typedef std::function< std::shared_ptr<Future>(Image** inputs, int nInputs, Image &output) > Function;
typedef std::function< std::shared_ptr<Future>(Image &input, Image &output) > Unary;
typedef std::function< std::shared_ptr<Future>(Image &a, Image &b, Image &output) > Binary;

Function convert(const Unary &f);
Function convert(const Binary &f);

std::shared_ptr<Future> callUnary(Function &f, Image &input, Image &output);
std::shared_ptr<Future> callBinary(Function &f, Image &a, Image &b, Image &output);

template <std::size_t N> std::shared_ptr<Future> call(Function &f, std::array<Image*, N> &inputs, Image &output) {
    return f(reinterpret_cast<Image**>(&inputs), N, output);
}
}
}
