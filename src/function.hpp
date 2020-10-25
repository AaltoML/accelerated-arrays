#pragma once

#include <array>
#include <functional>
#include <vector>

#include "future.hpp"
#include "assert.hpp"

namespace accelerated {
struct Image;

namespace operations {
typedef std::function< Future(Image** inputs, int nInputs, Image &output) > Function;
typedef std::function< Future(Image &output) > Nullary;
typedef std::function< Future(Image &input, Image &output) > Unary;
typedef std::function< Future(Image &a, Image &b, Image &output) > Binary;

Function convert(const Nullary &f);
Function convert(const Unary &f);
Function convert(const Binary &f);

Future callNullary(const Function &f, Image &output);
Future callUnary(const Function &f, Image &input, Image &output);
Future callBinary(const Function &f, Image &a, Image &b, Image &output);

template <std::size_t N> Future call(const Function &f, std::array<Image*, N> &inputs, Image &output) {
    return f(reinterpret_cast<Image**>(&inputs), N, output);
}

namespace sync {
typedef std::function< void(Image **inputs, int nInputs, Image &output) > Function;
typedef std::function< void(Image &output) > Nullary;
typedef std::function< void(Image &input, Image &output) > Unary;
typedef std::function< void(Image &a, Image &b, Image &output) > Binary;

template <class T, int N> Future wrapNAryBody(
    const std::function<void(T **inputs, int nInputs, T &output)> &syncFunc,
    Image **inputs, int nInputs, Image &output,
    Processor &p)
{
    (void)nInputs;
    aa_assert(nInputs == N);
    std::array<T*, N> args;
    for (int i = 0; i < nInputs; ++i) args[i] = &T::castFrom(*inputs[i]);
    auto &out = T::castFrom(output);
    return p.enqueue([syncFunc, args, &out]() {
        syncFunc(const_cast<T**>(reinterpret_cast<T* const*>(&args)), N, out);
    });
}

template <class T> Future wrapBody(
    const std::function<void(T **inputs, int nInputs, T &output)> &syncFunc,
    Image **inputs, int nInputs, Image &output,
    Processor &p)
{
    // optimizations for small N (typical cases)
    #define X(n) if (nInputs == n) return wrapNAryBody<T, n>(syncFunc, inputs, nInputs, output, p);
    X(0)
    X(1)
    X(2)
    #undef X

    std::vector<T*> args;
    args.reserve(nInputs);
    for (int i = 0; i < nInputs; ++i) args.push_back(&T::castFrom(*inputs[i]));
    auto &out = T::castFrom(output);
    return p.enqueue([syncFunc, args, &out]() {
        syncFunc(const_cast<T**>(reinterpret_cast<T* const*>(args.data())), args.size(), out);
    });
}

template <class T>
::accelerated::operations::Function
wrap(const std::function<void(T **inputs, int nInputs, T &output)> &syncFunc, Processor &p) {
    return [syncFunc, &p](Image **inputs, int nInputs, Image &output) -> Future {
        return wrapBody(syncFunc, inputs, nInputs, output, p);
    };
}

template <class T>
std::function<void(T **inputs, int nInputs, T &output)>
convert(const std::function<void(T &output)> &syncFunc) {
    return [syncFunc](T **inputs, int nInputs, T &output) {
        (void)inputs; (void)nInputs;
        aa_assert(nInputs == 0);
        syncFunc(output);
    };
}

template <class T>
std::function<void(T **inputs, int nInputs, T &output)>
convert(const std::function<void(T &input, T &output)> &syncFunc) {
    return [syncFunc](T **inputs, int nInputs, T &output) {
        aa_assert(nInputs == 1); (void)nInputs;
        syncFunc(*inputs[0], output);
    };
}

template <class T>
std::function<void(T **inputs, int nInputs, T &output)>
convert(const std::function<void(T &a, T &b, T &output)> &syncFunc) {
    return [syncFunc](T **inputs, int nInputs, T &output) {
        aa_assert(nInputs == 2); (void)nInputs;
        syncFunc(*inputs[0], *inputs[1], output);
    };
}

}

}
}
