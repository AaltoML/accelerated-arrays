#include "future.hpp"

namespace accelerated {
namespace {
struct InstantFuture : Future {
    void wait() final {};
};
}

Future::~Future() = default;
std::unique_ptr<Future> Future::instantlyResolved() {
    return std::unique_ptr<Future>(new InstantFuture);
}
}
