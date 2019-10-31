#pragma once

#include <nexus/fwd.hh>

#include <string>

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
    std::string const& name() const { return mName; }
    std::string const& file() const { return mFile; }
    int line() const { return mLine; }
    test_fun_t function() const { return mFunction; }
    bool isExclusive() const { return mIsExclusive; }

    // methods
public:
    void setExclusive() { mIsExclusive = true; }
    void addAfterPattern(std::string pattern) { mAfterPatterns.push_back(std::move(pattern)); }
    void addBeforePattern(std::string pattern) { mBeforePatterns.push_back(std::move(pattern)); }

    // ctor
public:
    Test(char const* name, char const* file, int line, test_fun_t fun) : mName(name), mFile(file), mLine(line), mFunction(fun) {}

    Test(Test const&) = delete;
    Test(Test&&) = delete;
    Test& operator=(Test const&) = delete;
    Test& operator=(Test&&) = delete;

    // members
private:
    std::string mName;
    std::string mFile;
    int mLine;
    test_fun_t mFunction;

    bool mIsExclusive = false;
    cc::vector<std::string> mAfterPatterns;
    cc::vector<std::string> mBeforePatterns;
};

namespace detail
{
cc::vector<cc::unique_ptr<Test>>& get_all_tests();
}
}
