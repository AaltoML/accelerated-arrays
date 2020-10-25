#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "future.hpp"
#include "assert.hpp"

namespace accelerated {
namespace {
struct BlockingQueue : Queue {
    virtual bool waitAndProcessOne() = 0;
    virtual void processUntilDestroyed() = 0;
};

class QueueImplementation : public BlockingQueue {
private:
    struct Task {
        std::unique_ptr<Promise> promise;
        std::function<void()> func;
    };

    std::deque< Task > tasks;
    std::mutex mutex;
    std::condition_variable emptyCondition, subscribeCondition;
    bool shouldQuit = false;
    int nSubscribed = 0;

    bool process(bool many, bool waitForData) {
        bool any = false;
        std::unique_lock<std::mutex> lock(mutex);
        nSubscribed++;
        subscribeCondition.notify_all();
        do {
            if (waitForData) emptyCondition.wait(lock, [this] {
                return shouldQuit || !tasks.empty();
            });
            if (shouldQuit || tasks.empty()) {
                break;
            }
            auto task = std::move(tasks.front());
            tasks.pop_front();
            lock.unlock();

            task.func();
            task.promise->resolve();
            any = true;

            lock.lock();
        } while (many);

        nSubscribed--;
        subscribeCondition.notify_all();
        return any;
    }

public:
    ~QueueImplementation() {
        std::unique_lock<std::mutex> lock(mutex);
        shouldQuit = true;
        emptyCondition.notify_all();

        subscribeCondition.wait(lock, [this] {
            return nSubscribed == 0;
        });
    }

    Future enqueue(const std::function<void()> &op) final {
        Task task;
        task.promise = Promise::create();
        auto future = task.promise->getFuture();
        task.func = op;

        {
            std::lock_guard<std::mutex> lock(mutex);
            tasks.emplace_back(std::move(task));
            emptyCondition.notify_one();
        }
        return future;
    }

    void waitUntilNSubscribed(int n) {
        // hacky
        std::unique_lock<std::mutex> lock(mutex);
        subscribeCondition.wait(lock, [this, n] {
            return nSubscribed == n;
        });
    }

    bool processOne() final { return process(false, false); }
    void processAll() final { process(true, false); }
    bool waitAndProcessOne() final { return process(false, true); }
    void processUntilDestroyed() final { process(true, true); }
};

struct ThreadPool : Processor {
private:
    std::vector< std::thread > pool;
    std::unique_ptr<QueueImplementation> queue;

    void work() {
        queue->processUntilDestroyed();
    }

public:
    ~ThreadPool() {
        queue->waitUntilNSubscribed(pool.size());
        queue.reset();
        for (auto &thread : pool) thread.join();
    }

    ThreadPool(int nThreads) : queue(new QueueImplementation) {
        aa_assert(nThreads > 0);
        for (int i = 0; i < nThreads; ++i) {
            pool.emplace_back([this]{ work(); });
        }
    }

    Future enqueue(const std::function<void()> &op) final {
        return queue->enqueue(op);
    }
};

struct InstantProcessor : Processor {
    Future enqueue(const std::function<void()> &op) final {
        op();
        return Future::instantlyResolved();
    }
};
}

std::unique_ptr<Processor> Processor::createInstant() {
    return std::unique_ptr<Processor>(new InstantProcessor);
}

std::unique_ptr<Processor> Processor::createThreadPool(int nThreads) {
    return std::unique_ptr<Processor>(new ThreadPool(nThreads));
}

std::unique_ptr<Queue> Processor::createQueue() {
    return std::unique_ptr<Queue>(new QueueImplementation);
}
}
