#pragma once

#include <clean-core/macros.hh>

#include <nexus/detail/api.hh>
#include <nexus/detail/enable_static_libraries_helper.hh>

#include "check.hh"
#include "config.hh"
#include "fwd.hh"

#ifndef NX_FORCE_MACRO_PREFIX

#define TEST(...) NX_TEST(__VA_ARGS__)

#endif

/**
 * defines a test case
 *
 * - first argument is a name, rest are optional configurations
 * - the same name may be reused many times
 *
 * Examples:
 *  TEST("my test")
 *  {
 *      CHECK(1 + 1 == 2);
 *  }
 *
 *  // is not run concurrently with any other test
 *  TEST("my test", exclusive) { ... }
 *
 *  // runs this test before/after some other tests (specified via pattern)
 *  // if the other tests fail for some reason, this test is not run at all
 *  TEST("my test", before("other test"), after("some pattern*")) { ... }
 */
#define NX_TEST(...) NX_DETAIL_REGISTER_TEST(CC_MACRO_JOIN(_nx_anon_test_function_, __COUNTER__), __VA_ARGS__)

// second layer to make sure function is expanded
#define NX_DETAIL_REGISTER_TEST(function, ...) NX_DETAIL_REGISTER_TEST2(function, __VA_ARGS__)

#define NX_DETAIL_REGISTER_TEST2(function, ...)                                                                                                        \
    static void function##_run();                                                                                                                      \
    static void function##_before()                                                                                                                    \
    {                                                                                                                                                  \
        ::nx::detail::number_of_assertions() = 0;                                                                                                      \
        ::nx::detail::number_of_failed_assertions() = 0;                                                                                               \
    }                                                                                                                                                  \
    static void function##_after(::nx::detail::local_check_counters* counters)                                                                         \
    {                                                                                                                                                  \
        counters->num_checks = ::nx::detail::number_of_assertions();                                                                                   \
        counters->num_failed_checks = ::nx::detail::number_of_failed_assertions();                                                                     \
    }                                                                                                                                                  \
    namespace                                                                                                                                          \
    {                                                                                                                                                  \
    struct _nx_register##function                                                                                                                      \
    {                                                                                                                                                  \
        ::nx::detail::local_check_counters m_counters = {};                                                                                            \
                                                                                                                                                       \
        _nx_register##function()                                                                                                                       \
        {                                                                                                                                              \
            nx::g_enable_static_libraries++;                                                                                                           \
            using namespace nx;                                                                                                                        \
            ::nx::detail::build_test(__FILE__, __LINE__, #function, &function##_run, &function##_before, &function##_after, &m_counters, __VA_ARGS__); \
        }                                                                                                                                              \
    } _nx_obj_register##function;                                                                                                                      \
    }                                                                                                                                                  \
    static void function##_run()

namespace nx
{
/// returns the seed to be used for the current test
/// NOTE: only valid inside a test!
NX_API size_t get_seed();

/// true iff the current test is in debug mode
/// NOTE: only valid inside a test!
NX_API bool is_current_test_debug();

/// true iff the current test is in reproduction mode
/// NOTE: only valid inside a test!
NX_API bool is_current_test_reproduction();

/// prints a reproduction string for the current test
/// NOTE: only works with MCTs currently
/// NOTE: only valid inside a test!
NX_API void print_current_test_reproduction();

namespace detail
{
NX_API Test* register_test(
    char const* name, char const* file, int line, char const* fun_name, test_fun_t fun, test_fun_before_t fun_before, test_fun_after_t fun_after, local_check_counters* counters);
NX_API void configure(Test* t, before const& v);
NX_API void configure(Test* t, after const& v);
NX_API void configure(Test* t, exclusive_t const&);
NX_API void configure(Test* t, should_fail_t const&);
NX_API void configure(Test* t, seed const& v);
NX_API void configure(Test* t, endless_t const&);
NX_API void configure(Test* t, reproduce const& r);
NX_API void configure(Test* t, disabled_t const&);
NX_API void configure(Test* t, debug_t const&);
NX_API void configure(Test* t, verbose_t const&);
NX_API void configure(Test* t, opt_in_group const& g);


template <class... Args>
void build_test(char const* file,
                int line,
                char const* fun_name,
                test_fun_t fun,
                test_fun_before_t fun_before,
                test_fun_after_t fun_after,
                local_check_counters* counters,
                char const* name,
                Args&&... args)
{
    [[maybe_unused]] auto t = register_test(name, file, line, fun_name, fun, fun_before, fun_after, counters);
    ((configure(t, args)), ...);
}
}
}
