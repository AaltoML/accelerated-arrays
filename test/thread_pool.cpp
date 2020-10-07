#include <catch2/catch.hpp>

#include <atomic>
#include "cpu/operations.hpp"

TEST_CASE( "Thread pool", "[accelerated-arrays]" ) {
    using namespace accelerated;

    for (unsigned itr = 0; itr < 20; ++itr) {
        auto processor = Processor::createThreadPool(10);
        std::atomic<int> val;
        val.store(0);
        std::vector<Future> parallelOps;

        for (unsigned j = 0; j < 5; ++j) {
            parallelOps.push_back(processor->enqueue([&val]() {
                val++;
            }));
        }

        for (unsigned j = 0; j < 5; ++j) {
            processor->enqueue([&val]() {
                val++;
            }).wait();
        }

        for (auto fut : parallelOps) fut.wait();
        REQUIRE(val.load() == 10);
    }
}
