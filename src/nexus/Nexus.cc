#include "Nexus.hh"

#include <nexus/apps/App.hh>
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
nx::App*& curr_app()
{
    thread_local static nx::App* a = nullptr;
    return a;
}
nx::Test*& curr_test()
{
    thread_local static nx::Test* t = nullptr;
    return t;
}
}

nx::App* nx::detail::get_current_app() { return curr_app(); }
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
    mTestArgC = argc - 1;
    mTestArgV = argv + 1;

    for (auto i = 1; i < argc; ++i)
    {
        auto s = cc::string_view(argv[i]);

        if (i == 1 && (s == "--help" || s == "-h"))
            mPrintHelp = true;

        if (s.empty() || s[0] == '-')
            continue; //  TODO

        mSpecificTests.push_back(s);
    }
}

int nx::Nexus::run()
{
    auto constexpr version = "0.0.1";

    if (mPrintHelp)
    {
        std::cout << "[nexus] version " << version << std::endl;
        std::cout << "NOTE: nexus is still early in development." << std::endl;
        std::cout << std::endl;
        std::cout << "usage:" << std::endl;
        std::cout << R"(  --help        shows this help)" << std::endl;
        std::cout << R"(  "test name"   runs all tests named "test name" (quotation marks optional if no space in name))" << std::endl;
        std::cout << std::endl;
        std::cout << "stats:" << std::endl;
        std::cout << "  - found " << detail::get_all_tests().size() << " tests" << std::endl;
        std::cout << "  - found " << detail::get_all_apps().size() << " apps" << std::endl;
        std::cout << std::endl;
        return EXIT_SUCCESS;
    }

    // apps
    cc::vector<App*> apps_to_run;
    for (auto const& app : detail::get_all_apps())
    {
        auto do_run = false;

        if (!mSpecificTests.empty())
        {
            for (auto const& s : mSpecificTests)
                if (s == app->name())
                    do_run = true;
        }

        if (do_run)
            apps_to_run.push_back(app.get());
    }
    if (!apps_to_run.empty())
    {
        for (auto const& a : apps_to_run)
        {
            a->mArgC = mTestArgC - 1;
            a->mArgV = mTestArgV + 1;
            curr_app() = a;
            a->function()(); // execute app
            curr_app() = nullptr;
        }
        return EXIT_SUCCESS;
    }

    // tests
    auto const& tests = detail::get_all_tests();

    auto seed = cc::make_hash(std::chrono::high_resolution_clock::now().time_since_epoch().count());

    cc::vector<Test*> tests_to_run;
    cc::vector<Test*> empty_tests;
    auto disabled_tests = 0;
    for (auto const& t : tests)
    {
        t->mArgC = mTestArgC - 1;
        t->mArgV = mTestArgV + 1;
        auto do_run = true;

        if (!t->isEnabled())
            do_run = false;

        if (!mSpecificTests.empty())
        {
            do_run = false;
            for (auto const& s : mSpecificTests)
                if (s == t->name())
                    do_run = true;
        }

        if (!do_run)
        {
            disabled_tests++;
            continue;
        }

        if (!t->mSeedOverwritten)
            t->mSeed = seed;

        tests_to_run.push_back(t.get());
    }

    std::cout << "[nexus] version " << version << std::endl;
    std::cout << "[nexus] run with '--help' for options" << std::endl;
    std::cout << "[nexus] detected " << tests.size() << (tests.size() == 1 ? " test" : " tests") << std::endl;
    std::cout << "[nexus] running " << tests_to_run.size() << (tests_to_run.size() == 1 ? " test" : " tests");
    if (disabled_tests > 0)
        std::cout << " (" << disabled_tests << " disabled)";
    std::cout << std::endl;
    std::cout << "[nexus] TEST(..., seed(" << seed << "))" << std::endl;
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
                std::cerr << "Test [" << t->name() << "] should have failed but didn't." << std::endl;
                std::cerr << "  in " << t->file() << ":" << t->line() << std::endl;
            }
        }
        else
        {
            if (t->hasFailed())
                fails++;
            failed_assertions += t->mFailedAssertions;
        }
        assertions += t->mAssertions;
        if (t->mAssertions == 0)
            empty_tests.push_back(t);

        // reset
        nx::detail::is_silenced() = false;
        nx::detail::always_terminate() = false;
        nx::detail::reset_assertion_handlers();

        // output
        auto const test_time_ms = std::chrono::duration<double>(end - start).count() * 1000;
        total_time_ms += test_time_ms;

        printf("  %-60s ... %6d checks in %.4f ms\n", t->name(), t->mAssertions, test_time_ms);
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
    {
        std::cerr << "[nexus] WARNING: no tests found/selected" << std::endl;
        return EXIT_SUCCESS;
    }
    else if (fails > 0)
    {
        for (auto const& t : tests)
            if (t->hasFailed() != t->shouldFail())
            {
                std::cerr << "[nexus] test [" << t->name() << "] failed (seed " << t->seed();
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
        return EXIT_FAILURE;
    }
    else
    {
        std::cout << "[nexus] success." << std::endl;
        if (!empty_tests.empty())
        {
            std::cerr << "[nexus] WARNING: " << empty_tests.size() << " test(s) have no assertions." << std::endl;
            std::cerr << "[nexus]   (this can indicate a bug and can be silenced with \"CHECK(true);\")" << std::endl;
            std::cerr << "[nexus]   affected tests:" << std::endl;
            for (auto t : empty_tests)
                std::cerr << "[nexus]   - [" << t->name() << "]" << std::endl;
        }

        return EXIT_SUCCESS;
    }
}
