#include "Nexus.hh"

#include <nexus/tests/Test.hh>

#include <clean-core/string_view.hh>
#include <clean-core/unique_ptr.hh>

#include <chrono>
#include <thread>

// TODO: proper log
#include <iomanip>
#include <iostream>

namespace
{
nx::Test*& curr_test()
{
    thread_local static nx::Test* t = nullptr;
    return t;
}
}
nx::Test* nx::detail::get_current_test() { return curr_test(); }

void nx::Nexus::applyCmdArgs(int argc, char** argv)
{
    for (auto i = 1; i < argc; ++i)
    {
        auto s = cc::string_view(argv[i]);
        if (s.empty() || s[0] == '-')
            continue; //  TODO

        mSpecificTests.push_back(s);
    }
}

int nx::Nexus::run()
{
    auto const& tests = detail::get_all_tests();

    cc::vector<Test*> tests_to_run;
    for (auto const& t : tests)
    {
        auto do_run = true;
        if (!mSpecificTests.empty())
        {
            do_run = false;
            for (auto const& s : mSpecificTests)
                if (s == t->name())
                    do_run = true;
        }

        if (!do_run)
            continue;

        tests_to_run.push_back(t.get());
    }

    std::cout << "[nexus] nexus version 0.0.1" << std::endl;
    std::cout << "[nexus] run with '--help' for options" << std::endl;
    std::cout << "[nexus] detected " << tests.size() << (tests.size() == 1 ? " test" : " tests") << std::endl;
    std::cout << "[nexus] running " << tests_to_run.size() << (tests_to_run.size() == 1 ? " test" : " tests") << std::endl;
    std::cout << "=======================================================" << std::endl;
    std::cout << std::setprecision(4);
    // TODO

    // execute tests
    // TODO: timings and statistics and so on
    auto total_time_ms = 0.0;
    for (auto t : tests_to_run)
    {
        curr_test() = t;
        auto const start_thread = std::this_thread::get_id();
        auto const start = std::chrono::high_resolution_clock::now();
        t->function()();
        auto const end = std::chrono::high_resolution_clock::now();
        auto const end_thread = std::this_thread::get_id();
        curr_test() = nullptr;

        auto const test_time_ms = std::chrono::duration<double>(end - start).count() * 1000;
        total_time_ms += test_time_ms;

        std::cout << "  [" << t->name().c_str() << "] ... in " << test_time_ms << " ms";

        if (start_thread != end_thread)
            std::cerr << " (WARNING: changed OS thread, from " << start_thread << " to " << end_thread << ")";

        std::cout << std::endl;
    }

    std::cout << "=======================================================" << std::endl;
    std::cout << "[nexus] passed " << tests.size() << (tests.size() == 1 ? " test" : " tests") << " in " << total_time_ms << " ms" << std::endl;
    std::cout << "[nexus] TODO!" << std::endl;

    return 0;
}
