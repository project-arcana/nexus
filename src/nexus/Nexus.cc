#include "Nexus.hh"

#include <nexus/tests/Test.hh>

#include <clean-core/unique_ptr.hh>

#include <chrono>

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
    // TODO

    // execute tests
    // TODO: timings and statistics and so on
    for (auto const& t : tests)
    {
        std::cout << "  [" << t->name() << "] ... ";
        std::cout.flush();
        auto start = std::chrono::high_resolution_clock::now();
        t->function()();
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << std::setprecision(4) << std::chrono::duration<double>(end - start).count() * 1000 << " ms" << std::endl;
    }

    std::cout << "=======================================================" << std::endl;
    std::cout << "[nexus] TODO!" << std::endl;

    return 0;
}
