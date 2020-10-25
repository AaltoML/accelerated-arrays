#include <future>

#include "future.hpp"
#include "assert.hpp"

namespace accelerated {
namespace {
struct InstantState : Future::State {
    void wait() final {};
};

class StdWrapper : public Future::State {
private:
    std::promise<void> p;
    std::future<void> f;

public:
    StdWrapper() : f(p.get_future()) {}

    void resolve() {
        p.set_value();
    }

    void wait() final {
        f.wait();
    }
};

class PromiseImplementation : public Promise {
private:
    std::shared_ptr<StdWrapper> wrapper;

public:
    PromiseImplementation() : wrapper(new StdWrapper) {}

    void resolve() final {
        wrapper->resolve();
    }

    Future getFuture() final {
        return Future(wrapper);
    }
};
}

Promise::~Promise() = default;
std::unique_ptr<Promise> Promise::create() {
    return std::unique_ptr<Promise>(new PromiseImplementation);
}

Future::Future(std::shared_ptr<State> state) : state(state) {}

Future::State::~State() = default;
Future Future::instantlyResolved() {
    return Future(std::unique_ptr<Future::State>(new InstantState));
}

void Future::wait() {
    aa_assert(state);
    return state->wait();
}

Processor::~Processor() = default;
}
