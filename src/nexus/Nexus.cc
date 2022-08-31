#include "Nexus.hh"

#include <nexus/apps/App.hh>
#include <nexus/check.hh>
#include <nexus/detail/assertions.hh>
#include <nexus/detail/exception.hh>
#include <nexus/detail/log.hh>
#include <nexus/tests/Test.hh>

#include <clean-core/hash.hh>
#include <clean-core/string_view.hh>
#include <clean-core/unique_ptr.hh>

#include <chrono>
#include <cstdlib>
#include <thread>

#include <rich-log/log.hh>

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
        LOG("version %s", version);
        LOG("");
        LOG("usage:");
        LOG(R"(  --help        shows this help)");
        LOG(R"(  "test name"   runs all tests named "test name" (quotation marks optional if no space in name))");
        LOG("");
        LOG("stats:");
        LOG(" - found %s tests", detail::get_all_tests().size());
        LOG(" - found %s apps", detail::get_all_apps().size());

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

    LOG("version %s", version);
    LOG("run with '--help' for options");
    LOG("detected %s %s", tests.size(), tests.size() == 1 ? "test" : "tests");
    LOG("running %s %s%s", tests_to_run.size(), tests_to_run.size() == 1 ? "test" : "tests",
         disabled_tests == 0 ? "" : cc::format(" (%s disabled)", disabled_tests));
    LOG("TEST(..., seed(%s))", seed);
    LOG("==============================================================================");

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

        // report
        curr_test() = nullptr;
        t->mAssertions = detail::number_of_assertions();
        t->mFailedAssertions = detail::number_of_failed_assertions();
        if (t->mShouldFail)
        {
            if (!t->hasFailed())
            {
                fails++;
                LOG_WARN("Test [%s] should have failed but didn't.\n  in %s:%s", t->name(), t->file(), t->line());
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

        LOG("  %<60s ... %6d checks in %.4f ms", t->name(), t->mAssertions, test_time_ms);
    }

    LOG("==============================================================================");
    LOG("passed %d of %d %s in %.4f ms%s", //
         tests_to_run.size() - fails, tests_to_run.size(), tests_to_run.size() == 1 ? "test" : "tests", total_time_ms,
         fails == 0 ? "" : cc::format(" (%d failed)", fails));
    LOG("checked %d assertions%s", assertions, failed_assertions == 0 ? "" : cc::format(" (%d failed)", failed_assertions));
    if (tests.empty())
    {
        LOG_WARN("no tests found/selected");
        return EXIT_SUCCESS;
    }
    else if (fails > 0)
    {
        for (auto const& t : tests)
            if (t->hasFailed() != t->shouldFail())
            {
                cc::string repr;
                if (t->shouldReproduce())
                {
                    repr += ", reproduce via TEST(..., reproduce(";
                    if (t->reproduction().trace.empty())
                        repr += cc::to_string(t->reproduction().seed);
                    else
                    {
                        repr += '"';
                        repr += t->reproduction().trace;
                        repr += '"';
                    }
                    repr += "))";
                }
                LOG_WARN("test [%s] failed (seed %d%s)", t->name(), t->seed(), repr);
            }
        LOG_WARN("%d %s failed", failed_assertions, failed_assertions == 1 ? "ASSERTION" : "ASSERTIONS");
        LOG_WARN("%d %s failed", fails, fails == 1 ? "TEST" : "TESTS");
        return EXIT_FAILURE;
    }
    else
    {
        LOG("success.");
        if (!empty_tests.empty())
        {
            cc::string warn_log;
            cc::format_to(warn_log, "%d test(s) have no assertions.\n", empty_tests.size());
            warn_log += "(this can indicate a bug and can be silenced with \"CHECK(true);\")\n";
            warn_log += "affected tests:\n";
            for (auto t : empty_tests)
            {
                cc::format_to(warn_log, "  - [%s]\n", t->name());
                cc::format_to(warn_log, "    in %s:%s\n", t->file(), t->line());
            }
            warn_log.pop_back();
            LOG_WARN("%s", warn_log);
        }

        return EXIT_SUCCESS;
    }
}
