#pragma once

#include <clean-core/typedefs.hh>

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

/// use a specific seed
struct seed
{
    explicit seed(size_t v) : value(v) {}
    size_t value;
};

/// used to reproduce a certain test
/// example:
///     reproduce(0xDEADBEEF)
struct reproduce
{
    explicit reproduce(size_t seed) : valid(true), seed(seed) {}

    bool valid = false;
    size_t seed = 0;
    // TODO: trace

    static reproduce none() { return {}; }

private:
    reproduce() = default;
};
}
