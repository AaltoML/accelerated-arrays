#pragma once

#include <memory>

namespace accelerated {

// This aims to conveniently package & abstract the ownership details of
// std::future (in case of truly asyncrhonous operations) while allowing to
// implement syncrhonus futures conveniently. Also the smart pointer stuff
// is encapsulated here for convenience and avoiding the ambiguity with
// ::get() (smart->raw pointer conversion vs wait for std::future)
struct Future {
    struct State {
        virtual ~State();
        virtual void wait() = 0;
    };
    std::shared_ptr<State> state;
    Future(std::unique_ptr<State> state);

    /** Block & wait until the operation is ready */
    void wait();

    static Future instantlyResolved();
};
}
