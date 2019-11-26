#include "Nexus.hh"

#include <nexus/check.hh>
#include <nexus/detail/assertions.hh>
#include <nexus/detail/exception.hh>
#include <nexus/tests/Test.hh>

#include <clean-core/hash.hh>
#include <clean-core/string_view.hh>
#include <clean-core/unique_ptr.hh>

#include <chrono>
#include <thread>

// TODO: proper log
#include <iomanip>
#include <iostream>
#include <sstream>

namespace
{
nx::Test*& curr_test()
{
    thread_local static nx::Test* t = nullptr;
    return t;
}
}
nx::Test* nx::detail::get_current_test() { return curr_test(); }
bool& nx::detail::is_silenced()
{
    thread_local static bool silenced = false;
    return silenced;
}
bool& nx::detail::always_terminate()
{
    thread_local static bool terminate = false;
    return terminate;
}

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

    auto seed = cc::make_hash(std::chrono::high_resolution_clock::now().time_since_epoch().count());

    cc::vector<Test*> tests_to_run;
    auto disabled_tests = 0;
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
        if (!t->isEnabled())
            do_run = false;

        if (!do_run)
        {
            disabled_tests++;
            continue;
        }

        if (!t->mSeedOverwritten)
            t->mSeed = seed;

        tests_to_run.push_back(t.get());
    }

    std::cout << "[nexus] nexus version 0.0.1" << std::endl;
    std::cout << "[nexus] run with '--help' for options" << std::endl;
    std::cout << "[nexus] detected " << tests.size() << (tests.size() == 1 ? " test" : " tests") << std::endl;
    std::cout << "[nexus] running " << tests_to_run.size() << (tests_to_run.size() == 1 ? " test" : " tests");
    if (disabled_tests > 0)
        std::cout << " (" << disabled_tests << " disabled)";
    std::cout << std::endl;
    std::cout << "[nexus] seed " << seed << std::endl;
    std::cout << "==============================================================================" << std::endl;
    std::cout << std::setprecision(4);
    // TODO

    // execute tests
    // TODO: timings and statistics and so on
    auto total_time_ms = 0.0;
    auto fails = 0;
    auto assertions = 0;
    auto failed_assertions = 0;
    for (auto t : tests_to_run)
    {
        // prepare
        detail::number_of_assertions() = 0;
        detail::number_of_failed_assertions() = 0;
        curr_test() = t;
        detail::is_silenced() = t->mShouldFail;
        detail::always_terminate() = false;

        // execute and measure
        auto const start_thread = std::this_thread::get_id();
        auto const start = std::chrono::high_resolution_clock::now();
        if (t->isDebug())
            t->function()();
        else
        {
            try
            {
                t->function()();
            }
            catch (nx::detail::assertion_failed_exception const&)
            {
                // empty by design
            }
        }
        auto const end = std::chrono::high_resolution_clock::now();
        auto const end_thread = std::this_thread::get_id();

        // report
        curr_test() = nullptr;
        t->mAssertions = detail::number_of_assertions();
        t->mFailedAssertions = detail::number_of_failed_assertions();
        if (t->mShouldFail)
        {
            if (!t->hasFailed())
            {
                fails++;
                std::cerr << "Test [" << t->name().c_str() << "] should have failed but didn't." << std::endl;
                std::cerr << "  in " << t->file().c_str() << ":" << t->line() << std::endl;
            }
        }
        else
        {
            if (t->hasFailed())
                fails++;
            failed_assertions += t->mFailedAssertions;
        }
        assertions += t->mAssertions;

        // reset
        nx::detail::is_silenced() = false;
        nx::detail::always_terminate() = false;
        nx::detail::reset_assertion_handlers();

        // output
        auto const test_time_ms = std::chrono::duration<double>(end - start).count() * 1000;
        total_time_ms += test_time_ms;

        std::stringstream ss_name, ss_asserts, ss_time;
        ss_name << "  [" << t->name().c_str() << "]";
        ss_asserts << " ... " << t->mAssertions << " assertions";
        ss_time << " in " << test_time_ms << " ms";
        auto s_name = ss_name.str();
        auto s_asserts = ss_asserts.str();
        auto s_time = ss_time.str();

        // TODO: faster
        while (s_name.size() < 40)
            s_name.push_back(' ');
        while (s_asserts.size() < 23)
            s_asserts = ' ' + s_asserts;

        std::cout << s_name << s_asserts << s_time << std::endl;

        if (start_thread != end_thread)
            std::cerr << " (WARNING: changed OS thread, from " << start_thread << " to " << end_thread << ")" << std::endl;
    }

    std::cout.flush();
    std::cerr.flush();
    std::cout << "==============================================================================" << std::endl;
    std::cout << "[nexus] passed " << tests_to_run.size() - fails << " of " << tests_to_run.size() << (tests_to_run.size() == 1 ? " test" : " tests")
              << " in " << total_time_ms << " ms";
    if (fails > 0)
        std::cout << " (" << fails << " failed)";
    std::cout << std::endl;
    std::cout << "[nexus] checked " << assertions << " assertions";
    if (failed_assertions > 0)
        std::cout << " (" << failed_assertions << " failed)";
    std::cout << std::endl;
    if (tests.empty())
        std::cerr << "[nexus] ERROR: no tests found/selected" << std::endl;
    else if (fails > 0)
    {
        for (auto const& t : tests)
            if (t->hasFailed() != t->shouldFail())
            {
                std::cerr << "[nexus] test [" << t->name().c_str() << "] failed (seed " << t->seed();
                if (t->shouldReproduce())
                {
                    std::cerr << ", reproduce via TEST(..., reproduce(";
                    if (t->reproduction().trace.empty())
                        std::cerr << t->reproduction().seed;
                    else
                        std::cerr << '"' << t->reproduction().trace.c_str() << '"';
                    std::cerr << "))";
                }
                std::cerr << ")" << std::endl;
            }
        std::cerr << "[nexus] ERROR: " << failed_assertions << " ASSERTION" << (failed_assertions == 1 ? "" : "S") << " FAILED" << std::endl;
        std::cerr << "[nexus] ERROR: " << fails << " TEST" << (fails == 1 ? "" : "S") << " FAILED" << std::endl;
    }
    else
        std::cout << "[nexus] success." << std::endl;

    return 0;
}
