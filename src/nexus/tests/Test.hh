#pragma once

#include <nexus/fwd.hh>

#include <clean-core/string.hh>
#include <clean-core/vector.hh>

namespace nx
{
/**
 * Represents a single test
 */
class Test
{
    // properties
public:
    cc::string const& name() const { return mName; }
    cc::string const& file() const { return mFile; }
    int line() const { return mLine; }
    test_fun_t function() const { return mFunction; }
    bool isExclusive() const { return mIsExclusive; }
    bool hasFailed() const { return mFailedAssertions > 0; }
    int assertions() const { return mAssertions; }
    int failedAssertions() const { return mFailedAssertions; }

    // methods
public:
    void setExclusive() { mIsExclusive = true; }
    void addAfterPattern(cc::string pattern) { mAfterPatterns.push_back(cc::move(pattern)); }
    void addBeforePattern(cc::string pattern) { mBeforePatterns.push_back(cc::move(pattern)); }

    // ctor
public:
    Test(char const* name, char const* file, int line, test_fun_t fun) : mName(name), mFile(file), mLine(line), mFunction(fun) {}

    Test(Test const&) = delete;
    Test(Test&&) = delete;
    Test& operator=(Test const&) = delete;
    Test& operator=(Test&&) = delete;

    // members
private:
    cc::string mName;
    cc::string mFile;
    int mLine;
    test_fun_t mFunction;

    int mAssertions = 0;
    int mFailedAssertions = 0;

    bool mIsExclusive = false;
    cc::vector<cc::string> mAfterPatterns;
    cc::vector<cc::string> mBeforePatterns;

    friend class Nexus;
};

namespace detail
{
cc::vector<cc::unique_ptr<Test>>& get_all_tests();
Test* get_current_test(); // TODO: discuss if and how this should be open API
}
}
