#include "Nexus.hh"

#include <nexus/apps/App.hh>
#include <nexus/check.hh>
#include <nexus/detail/assertions.hh>
#include <nexus/detail/exception.hh>
#include <nexus/detail/log.hh>
#include <nexus/tests/Test.hh>

#include <clean-core/defer.hh>
#include <clean-core/from_string.hh>
#include <clean-core/hash.hh>
#include <clean-core/string_view.hh>
#include <clean-core/unique_ptr.hh>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
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

cc::string to_timestamp(std::chrono::system_clock::time_point t)
{
    auto itt = std::chrono::system_clock::to_time_t(t);
    std::ostringstream ss;
    ss << std::put_time(gmtime(&itt), "%Y-%m-%dT%H:%M:%S");
    return ss.str().c_str();
}
cc::string current_timestamp() { return to_timestamp(std::chrono::system_clock::now()); }

cc::string repr_string_for(cc::string prefix, nx::Test const& t)
{
    cc::string repr;
    if (t.shouldReproduce())
    {
        repr += prefix;
        repr += "reproduce via TEST(..., reproduce(";
        if (t.reproduction().trace.empty())
            repr += cc::to_string(t.reproduction().seed);
        else
        {
            repr += '"';
            repr += t.reproduction().trace;
            repr += '"';
        }
        repr += "))";
    }
    return repr;
}
}

namespace nx
{
void write_xml_results(cc::string filename);
void write_xml_results_sentinel(cc::string filename);
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

    auto i = 1;
    for (auto i = 1; i < argc; ++i)
    {
        auto s = cc::string_view(argv[i]);

        if (i == 1 && (s == "--help" || s == "-h"))
            mPrintHelp = true;

        if (s == "--endless")
            mForceEndless = true;

        if (s == "--repr")
        {
            if (i + 1 < argc)
            {
                mForceReproduction = argv[i + 1];
                ++i;
            }
        }

        if (s == "--xml")
        {
            if (i + 1 < argc)
            {
                mXmlOutputFile = argv[i + 1];
                ++i;
            }
        }

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
        RICH_LOG("version %s", version);
        RICH_LOG("");
        RICH_LOG("usage:");
        RICH_LOG(R"(  --help        shows this help)");
        RICH_LOG(R"(  --endless     runs fuzz and mct tests in endless mode)");
        RICH_LOG(R"(  --repr s      runs a test reproduction (i.e. similar to reproduce(s)))");
        RICH_LOG(R"(  --xml file    writes the test results into the given file in JUnit xml style)");
        RICH_LOG(R"(  "test name"   runs all tests named "test name" (quotation marks optional if no space in name))");
        RICH_LOG("");
        RICH_LOG("stats:");
        RICH_LOG(" - found %s tests", detail::get_all_tests().size());
        RICH_LOG(" - found %s apps", detail::get_all_apps().size());

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

    // already write a dummy xml so a crash in nexus will be discovered
    if (!mXmlOutputFile.empty())
        nx::write_xml_results_sentinel(mXmlOutputFile);

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

        if (mForceEndless)
            t->mIsEndless = true;

        if (!mForceReproduction.empty())
        {
            // TODO: make this via 2 different cmd line args instead
            size_t s;
            if (cc::from_string(mForceReproduction, s))
                t->mReproduction = nx::reproduce(s);
            else
                t->mReproduction = nx::reproduce(mForceReproduction);
        }

        tests_to_run.push_back(t.get());
    }

    RICH_LOG("version %s", version);
    RICH_LOG("run with '--help' for options");
    RICH_LOG("detected %s %s", tests.size(), tests.size() == 1 ? "test" : "tests");
    RICH_LOG("running %s %s%s", tests_to_run.size(), tests_to_run.size() == 1 ? "test" : "tests",
             disabled_tests == 0 ? "" : cc::format(" (%s disabled)", disabled_tests));
    RICH_LOG("TEST(..., seed(%s))", seed);
    RICH_LOG("==============================================================================");

    // execute tests
    // TODO: timings and statistics and so on
    auto total_time_ms = 0.0;
    auto num_failed_tests = 0;
    auto total_num_checks = 0;
    auto total_num_failed_checks = 0;

    for (auto* t : tests_to_run)
    {
        // prepare
        curr_test() = t;
        detail::is_silenced() = t->mShouldFail;
        detail::always_terminate() = false;

        auto const timestamp = current_timestamp();
        t->mFunctionBefore();

        // execute and measure
        auto const start = std::chrono::high_resolution_clock::now();
        if (t->isDebug() || t->shouldReproduce())
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

        t->mFunctionAfter(t->mCounters);

        // report
        curr_test() = nullptr;

        auto const num_checks = t->mCounters->num_checks;
        auto const num_failed_checks = t->mCounters->num_failed_checks;

        total_num_checks += num_checks;

        t->setDidFail(num_failed_checks > 0);

        if (t->mShouldFail)
        {
            if (!t->didFail())
            {
                num_failed_tests++;
                RICH_LOG_WARN("Test [%s] should have failed but didn't.\n  in %s:%s", t->name(), t->file(), t->line());
            }
        }
        else
        {
            if (t->didFail())
                num_failed_tests++;

            total_num_failed_checks += num_failed_checks;
        }


        if (num_checks == 0)
            empty_tests.push_back(t);

        // reset
        nx::detail::is_silenced() = false;
        nx::detail::always_terminate() = false;
        nx::detail::reset_assertion_handlers();

        // output
        auto const test_time_ms = std::chrono::duration<double>(end - start).count() * 1000;
        total_time_ms += test_time_ms;
        t->setExecutionTime(timestamp, test_time_ms / 1000);

        RICH_LOG("  %<60s ... %6d checks in %.4f ms", t->name(), num_checks, test_time_ms);
    }

    RICH_LOG("==============================================================================");
    RICH_LOG("passed %d of %d %s in %.4f ms%s", //
             tests_to_run.size() - num_failed_tests, tests_to_run.size(), tests_to_run.size() == 1 ? "test" : "tests", total_time_ms,
             num_failed_tests == 0 ? "" : cc::format(" (%d failed)", num_failed_tests));
    RICH_LOG("checked %d assertions%s", total_num_checks, total_num_failed_checks == 0 ? "" : cc::format(" (%d failed)", total_num_failed_checks));

    CC_DEFER
    {
        if (!mXmlOutputFile.empty())
            nx::write_xml_results(mXmlOutputFile);
    };

    if (tests.empty())
    {
        RICH_LOG_WARN("no tests found/selected");
        return EXIT_SUCCESS;
    }
    else if (num_failed_tests > 0)
    {
        for (auto const& t : tests)
            if (t->didFail() != t->shouldFail())
            {
                RICH_LOG_WARN("test [%s] failed (seed %d%s)", t->name(), t->seed(), repr_string_for(", ", *t));
                RICH_LOG_WARN("  %s:%s", t->file(), t->line());
            }

        RICH_LOG_WARN("%d %s failed", total_num_failed_checks, total_num_failed_checks == 1 ? "ASSERTION" : "ASSERTIONS");
        RICH_LOG_WARN("%d %s failed", num_failed_tests, num_failed_tests == 1 ? "TEST" : "TESTS");

        return EXIT_FAILURE;
    }
    else
    {
        RICH_LOG("success.");
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
            RICH_LOG_WARN("%s", warn_log);
        }

        return EXIT_SUCCESS;
    }
}

void nx::write_xml_results(cc::string filename)
{
    // see https://github.com/testmoapp/junitxml
    cc::string xml;

    auto const timestamp = current_timestamp();

    auto const& tests = detail::get_all_tests();

    auto total_tests = 0;
    auto total_errors = 0;   // aka abnormal executions
    auto total_failures = 0; // aka failed check
    auto total_skipped = 0;  // aka disabled
    auto total_assertions = 0;
    double total_time = 0;

    for (auto const& t : tests)
    {
        ++total_tests;

        if (!t->isEnabled())
        {
            ++total_skipped;
            continue;
        }

        if (t->didFail() != t->shouldFail())
            ++total_failures;

        total_assertions += t->numberOfChecks();
        total_time += t->executionTimeInSec();
    }

    auto escapeXmlString = [](cc::string name) -> cc::string
    {
        cc::string s;
        for (auto c : name)
        {
            switch (c)
            {
            case '<':
                s += "&lt;";
                break;
            case '>':
                s += "&gt;";
                break;
            case '&':
                s += "&amp;";
                break;
            case '"':
                s += "&quot;";
                break;
            case '\'':
                s += "&apos;";
                break;

            default:
                s += c;
            }
        }
        return s;
    };

    xml += R"(<?xml version="1.0" encoding="UTF-8"?>)";
    xml += cc::format(R"(<testsuites name="Test run" tests="%s" failures="%s" errors="%s" skipped="%s" assertions="%s" time="%.5f" timestamp="%s">)",
                      total_tests, total_failures, 0, total_skipped, total_assertions, total_time, timestamp);
    xml += cc::format(R"(<testsuite name="Test run" tests="%s" failures="%s" errors="%s" skipped="%s" assertions="%s" time="%.5f" timestamp="%s">)",
                      total_tests, total_failures, 0, total_skipped, total_assertions, total_time, timestamp);
    for (auto const& t : tests)
    {
        xml += cc::format(R"(<testcase name="%s" assertions="%s" time="%.5f" file="%s" line="%s">)", escapeXmlString(t->name()), t->numberOfChecks(),
                          t->executionTimeInSec(), escapeXmlString(t->file()), t->line());
        if (!t->isEnabled())
        {
            xml += R"(<skipped message="Test is disabled" />)";
        }
        else if (t->didFail() && !t->shouldFail())
        {
            xml += cc::format(R"(<failure message="%s">%s</failure>)", escapeXmlString(t->makeFirstFailMessage()), escapeXmlString(t->makeFirstFailInfo()));
        }
        else if (!t->didFail() && t->shouldFail())
        {
            xml += R"(<failure message="Test did not fail but was marked as should_fail."></failure>)";
        }
        xml += R"(</testcase>)";
    }
    xml += R"(</testsuite>)";
    xml += R"(</testsuites>)";

    std::ofstream(filename.c_str()) << xml.c_str();
    LOG("wrote xml result to '%s'", filename);
}

void nx::write_xml_results_sentinel(cc::string filename)
{
    // see https://github.com/testmoapp/junitxml
    cc::string xml;

    auto const timestamp = current_timestamp();

    xml += R"(<?xml version="1.0" encoding="UTF-8"?>)";
    xml += cc::format(R"(<testsuites name="Test run" tests="1" failures="0" errors="1" skipped="0" assertions="1" time="0.0" timestamp="%s">)", timestamp);
    xml += cc::format(R"(<testsuite name="Test run" tests="1" failures="0" errors="1" skipped="0" assertions="1" time="0.0" timestamp="%s">)", timestamp);
    xml += R"(<testcase name="Dummy Test Case" assertions="1" time="0" file="does-not-exist.cc" line="1">)";
    xml += R"(<failure message="Nexus did not run until real xml was written. This indicates a hard crash inside the test framework."></failure>)";
    xml += R"(</testcase>)";
    xml += R"(</testsuite>)";
    xml += R"(</testsuites>)";

    std::ofstream(filename.c_str()) << xml.c_str();
}
