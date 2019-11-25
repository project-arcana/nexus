#pragma once

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
}
