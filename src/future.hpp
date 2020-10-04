#pragma once

#include <memory>

namespace accelerated {

// This abstraction / packaging of std::future<void> is useful in order to
// ensure that both the future and its associated promise stay alive
// as long as necessary. It also simplifies the implementation of
// syncrhonous operations
struct Future {
    virtual ~Future();
    /** Block & wait until the operation is ready */
    virtual void wait() = 0;

    static std::unique_ptr<Future> instantlyResolved();
};

}
