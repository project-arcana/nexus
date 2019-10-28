#include "Nexus.hh"

#include <nexus/tests/Test.hh>

#include <clean-core/unique_ptr.hh>

#include <chrono>
#include <thread>

// TODO: proper log
#include <iomanip>
#include <iostream>

void nx::Nexus::applyCmdArgs(int argc, char** argv)
{
    // TODO
}

int nx::Nexus::run()
{
    auto const& tests = detail::get_all_tests();

    std::cout << "[nexus] nexus version 0.0.1" << std::endl;
    std::cout << "[nexus] run with '--help' for options" << std::endl;
    std::cout << "[nexus] detected " << tests.size() << (tests.size() == 1 ? " test" : " tests") << std::endl;
    std::cout << "=======================================================" << std::endl;
    std::cout << std::setprecision(4);
    // TODO

    // execute tests
    // TODO: timings and statistics and so on
    auto total_time_ms = 0.0;
    for (auto const& t : tests)
    {
        std::cout << "  [" << t->name() << "] ... ";
        // std::cout.flush(); // TODO: Causes formatting errors in some scenarios

        auto const start_thread = std::this_thread::get_id();
        auto const start = std::chrono::high_resolution_clock::now();
        t->function()();
        auto const end = std::chrono::high_resolution_clock::now();
        auto const end_thread = std::this_thread::get_id();

        auto const test_time_ms = std::chrono::duration<double>(end - start).count() * 1000;
        total_time_ms += test_time_ms;

        std::cout << test_time_ms << " ms" << std::endl;

        if (start_thread != end_thread)
        {
            std::cerr << "  [" << t->name() << "] warning: changed OS thread (from " << start_thread << " to " << end_thread << ")" << std::endl;
        }
    }

    std::cout << "=======================================================" << std::endl;
    std::cout << "[nexus] passed " << tests.size() << (tests.size() == 1 ? " test" : " tests") << " in " << total_time_ms << " ms" << std::endl;
    std::cout << "[nexus] TODO!" << std::endl;

    return 0;
}
