#pragma once

#include <nexus/fwd.hh>

#include <clean-core/string.hh>
#include <clean-core/vector.hh>

#include <nexus/config.hh>

namespace nx
{
/**
 * Represents a single test
 */
class Test
{
    // properties
public:
    char const* name() const { return mName; }
    char const* file() const { return mFile; }
    int line() const { return mLine; }
    char const* functionName() const { return mFunctionName; }
    test_fun_t function() const { return mFunction; }
    int argc() const { return mArgC; }
    char const* const* argv() const { return mArgV; }
    cc::span<char const* const> argSpan() const { return {mArgV, size_t(mArgC)}; }

    size_t seed() const { return mSeed; }
    bool isExclusive() const { return mIsExclusive; }
    bool hasFailed() const { return mFailedAssertions > 0; }
    int assertions() const { return mAssertions; }
    int failedAssertions() const { return mFailedAssertions; }
    bool shouldFail() const { return mShouldFail; }
    bool isEndless() const { return mIsEndless; }
    bool shouldReproduce() const { return mReproduction.valid; }
    reproduce reproduction() const { return mReproduction; }
    bool isEnabled() const { return mIsEnabled; }
    bool isDebug() const { return mIsDebug; }
    bool isVerbose() const { return mIsVerbose; }

    // methods
public:
    void setExclusive() { mIsExclusive = true; }
    void setShouldFail() { mShouldFail = true; }
    void setEndless() { mIsEndless = true; }
    void setDisabled() { mIsEnabled = false; }
    void setDebug() { mIsDebug = true; }
    void setVerbose() { mIsVerbose = true; }
    void setReproduce(reproduce r) { mReproduction = r; }
    void addAfterPattern(cc::string pattern) { mAfterPatterns.push_back(cc::move(pattern)); }
    void addBeforePattern(cc::string pattern) { mBeforePatterns.push_back(cc::move(pattern)); }

    void overwriteSeed(size_t s)
    {
        mSeed = s;
        mSeedOverwritten = true;
    }

    // ctor
public:
    Test(char const* name, char const* file, int line, char const* fun_name, test_fun_t fun)
      : mName(name), mFile(file), mLine(line), mFunctionName(fun_name), mFunction(fun)
    {
        CC_CONTRACT(name);
    }

    Test(Test const&) = delete;
    Test(Test&&) = delete;
    Test& operator=(Test const&) = delete;
    Test& operator=(Test&&) = delete;

    // members
private:
    char const* mName;         // name of the test as given in the macro, ie. TEST("name") { .. }
    char const* mFile;         // filename where the test is defined, directly from __FILE__ during registration
    int mLine;                 // line where the test is declared in the file, directly from __LINE__
    char const* mFunctionName; // macro-stringified name of the function, ie. "_nx_anon_test_function_5")
    test_fun_t mFunction;      // function pointer to the entry
    size_t mSeed;              // RNG seed, unintialized or user-provided if mSeedOverwritten = true

    int mAssertions = 0;
    int mFailedAssertions = 0;

    bool mIsExclusive = false;
    bool mShouldFail = false;
    bool mSeedOverwritten = false;
    bool mIsEndless = false;
    bool mIsEnabled = true;
    bool mIsDebug = false;
    bool mIsVerbose = false;

    int mArgC = 0;
    char const* const* mArgV = nullptr;

    reproduce mReproduction = reproduce::none();

    cc::vector<cc::string> mAfterPatterns;
    cc::vector<cc::string> mBeforePatterns;

    friend class Nexus;
};

namespace detail
{
cc::vector<cc::unique_ptr<Test>>& get_all_tests();
Test* get_current_test(); // TODO: discuss if and how this should be open API
bool& is_silenced();      // TODO: discuss if and how this should be open API
bool& always_terminate(); // TODO: discuss if and how this should be open API
}
}
