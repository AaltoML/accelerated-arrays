#pragma once

#include <memory>
#include <functional>

namespace accelerated {

// Allows implementing both syncrhonous and asynchronus operations conveniently
// Smart pointer stuff is encapsulated here for convenience and avoiding the
// ambiguity of future.get() (smart->raw pointer conversion vs wait)
struct Future {
    struct State {
        virtual ~State();
        virtual void wait() = 0;
    };

    std::shared_ptr<State> state;
    Future(std::shared_ptr<State> state);

    /** Block & wait until the operation is ready */
    void wait();

    static Future instantlyResolved();
};

// Packages std::future & std::promise to avoid non-trivial lifetime issues
struct Promise {
    virtual ~Promise();
    static std::unique_ptr<Promise> create();

    virtual void resolve() = 0;
    virtual Future getFuture() = 0;
};

struct Queue;
struct Processor {
    virtual ~Processor();
    virtual Future enqueue(const std::function<void()> &op) = 0;

    static std::unique_ptr<Processor> createInstant();
    static std::unique_ptr<Processor> createThreadPool(int nThreads);
    static std::unique_ptr<Queue> createQueue();
};

struct Queue : Processor {
    virtual bool processOne() = 0;
    virtual void processAll() = 0;
};
}
