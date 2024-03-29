#pragma once

#include <cstddef>

#include <clean-core/move.hh>
#include <clean-core/string.hh>

namespace nx
{
/// runs the test before all tests matching the pattern
/// if this tests fails, no matched test is run
struct before
{
    char const* pattern;
};

/// runs the test after all tests matching the pattern
/// if any matched test fails, this test is not run
struct after
{
    char const* pattern;
};

/// only executes the test if cmd line arg "--group name" is passed
struct opt_in_group
{
    char const* name;

    explicit opt_in_group(char const* name) : name(name) {}
};

/// this test is not run concurrently with any other test (for multi-threaded execution)
static constexpr struct exclusive_t
{
} exclusive;

/// this test should have failed assertions
static constexpr struct should_fail_t
{
} should_fail;

/// this test runs endlessly
static constexpr struct endless_t
{
} endless;

/// this test is disabled
static constexpr struct disabled_t
{
} disabled;

/// don't catch errors thrown (for debugging)
static constexpr struct debug_t
{
} debug;

/// verbose test execution (for debugging)
/// (especially useful for debugging endless loops in a MCT)
static constexpr struct verbose_t
{
} verbose;

/// use a specific seed
struct seed
{
    explicit seed(size_t v) : value(v) {}
    size_t value;
};

/// used to reproduce a certain test
/// example:
///     reproduce(0xDEADBEEF)
///     reproduce("some trace")
struct reproduce
{
    explicit reproduce(size_t seed) : valid(true), seed(seed) {}
    explicit reproduce(cc::string trace) : valid(true), trace(cc::move(trace)) {}

    bool valid = false;
    size_t seed = 0;
    cc::string trace;

    static reproduce none() { return {}; }

private:
    reproduce() = default;
};
}
