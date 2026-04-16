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
#include <iostream>
#include <sstream>
#include <thread>

#include <rich-log/log.hh>
#include <rich-log/logger.hh>

#define SCOL_GRAY "\u001b[38;5;244m"
#define SCOL_ORANGE "\u001b[38;5;220m"
#define SCOL_RED "\u001b[38;5;196m"
#define SCOL_RESET "\u001b[0m"

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

cc::string colored_test_time_str(double time_ms)
{
    auto s = cc::format("%7.2f ms", time_ms);
    if (time_ms < 10)
        s = SCOL_GRAY + s + SCOL_RESET;
    else if (time_ms < 100)
        s = SCOL_RESET + s;
    else if (time_ms < 1000)
        s = SCOL_ORANGE + s + SCOL_RESET;
    else
        s = SCOL_RED + s + SCOL_RESET;
    return s;
}

cc::string escape_xml(cc::string_view name)
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
}

cc::string escape_json(cc::string_view s)
{
    cc::string out;
    for (auto c : s)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
        }
    }
    return out;
}

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
void write_catch2_discovery_xml(cc::vector<Test*> const& tests);
void write_catch2_results_xml(cc::vector<Test*> const& tests);
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

    // Pre-scan: detect catch2 mode before processing positional args
    // so filters appearing before --list-tests/--reporter are handled correctly
    for (auto i = 1; i < argc; ++i)
    {
        auto s = cc::string_view(argv[i]);
        if (s == "--list-tests" || s == "--reporter")
            mCatch2Mode = true;
    }

    for (auto i = 1; i < argc; ++i)
    {
        auto s = cc::string_view(argv[i]);

        if (i == 1 && (s == "--help" || s == "-h"))
            mPrintHelp = true;

        if (s == "--verbose" || s == "-v")
            mVerbose = true;

        if (s == "--endless")
            mForceEndless = true;

        if (s == "--no-endless")
            mNoEndless = true;

        if (s == "--repr")
        {
            if (i + 1 < argc)
            {
                mForceReproduction = argv[i + 1];
                ++i;
            }
        }

        if (s == "--group" || s == "-g")
        {
            if (i + 1 < argc)
            {
                mEnabledGroups.push_back(argv[i + 1]);
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

        if (s == "--list-json")
        {
            mListJson = true;
            continue;
        }

        // Catch2 compat flags
        if (s == "--list-tests")
        {
            mCatch2Mode = true;
            mHasListTests = true;
            continue;
        }

        if (s == "--reporter")
        {
            mCatch2Mode = true;
            mHasXmlReporter = true;
            if (i + 1 < argc)
                ++i; // consume reporter value
            continue;
        }

        if (s == "--verbosity" || s == "--durations")
        {
            if (i + 1 < argc)
                ++i; // consume value
            continue;
        }

        if (s == "-v") // already handled above
            continue;

        if (s.empty() || s[0] == '-')
            continue; //  TODO

        // positional arg: treat as test name filter
        // in Catch2 mode: comma-split and unescape \[ -> [
        if (mCatch2Mode)
        {
            // split on comma
            auto remaining = s;
            while (!remaining.empty())
            {
                auto comma = remaining.index_of(',');
                cc::string filter;
                if (comma == -1)
                {
                    filter = remaining;
                    remaining = {};
                }
                else
                {
                    filter = remaining.subview(0, comma);
                    remaining = remaining.subview(comma + 1);
                }

                // unescape \[ -> [
                cc::string unescaped;
                for (size_t j = 0; j < filter.size(); ++j)
                {
                    if (filter[j] == '\\' && j + 1 < filter.size() && filter[j + 1] == '[')
                    {
                        unescaped += '[';
                        ++j;
                    }
                    else
                        unescaped += filter[j];
                }

                // special-case catch2 filter semantics
                if (unescaped == "[.]")
                    mRunDisabledTests = true; // [.] = hidden/disabled tests tag
                else if (unescaped != "*" && !unescaped.empty())
                    mSpecificTests.push_back(cc::move(unescaped)); // * = all tests, skip filter
            }
        }
        else
        {
            mSpecificTests.push_back(s);
        }
    }
}

int nx::Nexus::run()
{
    auto constexpr version = "0.0.1";

    if (mPrintHelp)
    {
        RICH_LOG("version %s", version);
        // TestMate detects this line to identify the binary as Catch2-compatible
        RICH_LOG("Compatible with Catch2 v3.11.0 in some args");
        RICH_LOG("");
        RICH_LOG("usage:");
        RICH_LOG(R"(  --help        shows this help)");
        RICH_LOG(R"(  --verbose/-v  prints a header/footer around each test (useful for debugging hangs))");
        RICH_LOG(R"(  --endless     runs fuzz and mct tests in endless mode)");
        RICH_LOG(R"(  --no-endless  errors if any test would be run in endless mode (useful for CI))");
        RICH_LOG(R"(  --repr s      runs a test reproduction (i.e. similar to reproduce(s)))");
        RICH_LOG(R"(  --xml file    writes the test results into the given file in JUnit xml style)");
        RICH_LOG(R"(  --list-json   lists all tests as JSON to stdout and exits)");
        RICH_LOG(R"(  "test name"   runs all tests named "test name" (quotation marks optional if no space in name))");
        RICH_LOG("");
        RICH_LOG("stats:");
        RICH_LOG(" - found %s tests", detail::get_all_tests().size());
        RICH_LOG(" - found %s apps", detail::get_all_apps().size());

        return EXIT_SUCCESS;
    }

    // JSON discovery mode: list ALL registered tests with metadata as JSON and exit
    if (mListJson)
    {
        auto const& all_tests = detail::get_all_tests();
        cc::string json;
        json += "[\n";
        for (size_t i = 0; i < all_tests.size(); ++i)
        {
            auto const* t = all_tests[i].get();
            json += "  {\n";
            json += cc::format("    \"name\": \"%s\",\n", escape_json(t->name()));
            json += cc::format("    \"file\": \"%s\",\n", escape_json(t->file()));
            json += cc::format("    \"line\": %s,\n", t->line());
            json += cc::format("    \"enabled\": %s,\n", t->isEnabled() ? "true" : "false");
            json += cc::format("    \"exclusive\": %s,\n", t->isExclusive() ? "true" : "false");
            json += cc::format("    \"should_fail\": %s,\n", t->shouldFail() ? "true" : "false");
            json += cc::format("    \"endless\": %s\n", t->isEndless() ? "true" : "false");
            json += i + 1 < all_tests.size() ? "  },\n" : "  }\n";
        }
        json += "]\n";
        std::cout << json.c_str();
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

        if (!t->isEnabled() && !mRunDisabledTests)
            do_run = false;

        if (!t->mOptInGroups.empty())
        {
            auto enabled = false;
            for (auto const& g : mEnabledGroups)
                if (t->mOptInGroups.contains(g))
                    enabled = true;
            if (!enabled)
                do_run = false;
        }

        // AFTER opt-in groups, so you can still selectively run them
        if (!mSpecificTests.empty())
        {
            do_run = false;
            for (auto const& s : mSpecificTests)
            {
                if (mCatch2Mode ? cc::string_view(t->name()).contains(cc::string_view(s)) : (s == t->name()))
                    do_run = true;
            }
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

        if (t->mIsEndless && mNoEndless)
        {
            LOG_ERROR("test '%s' would be run in endless more but --no-endless is specified", t->name());
            return EXIT_FAILURE;
        }

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

    // Catch2 discovery mode: list tests as XML and exit
    if (mHasListTests && mHasXmlReporter)
    {
        nx::write_catch2_discovery_xml(tests_to_run);
        return EXIT_SUCCESS;
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
        t->clearFailedChecks();

        auto const timestamp = current_timestamp();

        if (mVerbose)
            RICH_LOG("[running \"%s\" %s:%s]", t->name(), t->file(), t->line());

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

        if (mVerbose)
        {
            auto const failed = (t->didFail() != t->shouldFail());
            RICH_LOG("[%s in %s (%d %s)]", failed ? "FAILED" : "success", colored_test_time_str(test_time_ms), num_checks, num_checks == 1 ? "check" : "checks");
            RICH_LOG("");
        }
        else
        {
            RICH_LOG("  %<60s " SCOL_GRAY "... " SCOL_RESET "%7d" SCOL_GRAY " checks in %s", //
                     t->name(), num_checks, colored_test_time_str(test_time_ms));
        }
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

    // Catch2 results mode: emit <TestRun> XML to stdout
    if (mHasXmlReporter && !mHasListTests)
        nx::write_catch2_results_xml(tests_to_run);

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
    [[maybe_unused]] auto total_errors = 0; // aka abnormal executions
    auto total_failures = 0;                // aka failed check
    auto total_skipped = 0;                 // aka disabled
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

    xml += R"(<?xml version="1.0" encoding="UTF-8"?>)";
    xml += cc::format(R"(<testsuites name="Test run" tests="%s" failures="%s" errors="%s" skipped="%s" assertions="%s" time="%.5f" timestamp="%s">)",
                      total_tests, total_failures, 0, total_skipped, total_assertions, total_time, timestamp);
    xml += cc::format(R"(<testsuite name="Test run" tests="%s" failures="%s" errors="%s" skipped="%s" assertions="%s" time="%.5f" timestamp="%s">)",
                      total_tests, total_failures, 0, total_skipped, total_assertions, total_time, timestamp);
    for (auto const& t : tests)
    {
        xml += cc::format(R"(<testcase name="%s" assertions="%s" time="%.5f" file="%s" line="%s">)", escape_xml(t->name()), t->numberOfChecks(),
                          t->executionTimeInSec(), escape_xml(t->file()), t->line());
        if (!t->isEnabled())
        {
            xml += R"(<skipped message="Test is disabled" />)";
        }
        else if (t->didFail() && !t->shouldFail())
        {
            xml += cc::format(R"(<failure message="%s">%s</failure>)", escape_xml(t->makeFirstFailMessage()), escape_xml(t->makeFirstFailInfo()));
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

void nx::write_catch2_discovery_xml(cc::vector<Test*> const& tests)
{
    cc::string xml;
    xml += R"(<?xml version="1.0" encoding="UTF-8"?>)";
    xml += "\n<MatchingTests>\n";
    for (auto const* t : tests)
    {
        xml += "  <TestCase>\n";
        xml += cc::format("    <Name>%s</Name>\n", escape_xml(t->name()));
        xml += "    <ClassName/>\n";
        xml += cc::format("    <Tags>%s</Tags>\n", t->isEnabled() ? "" : "[.]");
        xml += "    <SourceInfo>\n";
        xml += cc::format("      <File>%s</File>\n", escape_xml(t->file()));
        xml += cc::format("      <Line>%s</Line>\n", t->line());
        xml += "    </SourceInfo>\n";
        xml += "  </TestCase>\n";
    }
    xml += "</MatchingTests>\n";
    std::cout << xml.c_str();
}

void nx::write_catch2_results_xml(cc::vector<Test*> const& tests)
{
    cc::string xml;
    xml += R"(<?xml version="1.0" encoding="UTF-8"?>)";
    xml += "\n<TestRun>\n";
    for (auto const* t : tests)
    {
        xml += cc::format(R"(  <TestCase name="%s" filename="%s" line="%s">)", escape_xml(t->name()), escape_xml(t->file()), t->line());
        xml += "\n";

        if (!t->isEnabled())
        {
            xml += "    <Skipped/>\n";
        }
        else
        {
            for (auto const& fc : t->failedChecks())
            {
                xml += cc::format(R"(    <Expression success="false" filename="%s" line="%s">)", escape_xml(fc.file), fc.line);
                xml += "\n";
                xml += cc::format("      <Original>%s</Original>\n", escape_xml(fc.original));
                xml += cc::format("      <Expanded>%s</Expanded>\n", escape_xml(fc.expanded));
                xml += "    </Expression>\n";
            }

            auto const success = !t->didFail();
            xml += cc::format(R"(    <OverallResult success="%s" durationInSeconds="%.7f"/>)", success ? "true" : "false", t->executionTimeInSec());
            xml += "\n";
        }
        xml += "  </TestCase>\n";
    }
    xml += "</TestRun>\n";
    std::cout << xml.c_str();
}
