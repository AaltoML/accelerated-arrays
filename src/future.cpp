#include <cassert>

#include "future.hpp"

namespace accelerated {
namespace {
struct InstantState : Future::State {
    void wait() final {};
};
}

Future::Future(std::unique_ptr<State> state) : state(std::move(state)) {}

Future::State::~State() = default;
Future Future::instantlyResolved() {
    return Future(std::unique_ptr<Future::State>(new InstantState));
}

void Future::wait() {
    assert(state);
    return state->wait();
}
}
