#pragma once

#include <clean-core/macros.hh>

#include <nexus/detail/api.hh>

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

#define NX_DETAIL_REGISTER_TEST2(function, ...)                                              \
    static void function();                                                                  \
    namespace                                                                                \
    {                                                                                        \
    struct _nx_register##function                                                            \
    {                                                                                        \
        _nx_register##function()                                                             \
        {                                                                                    \
            using namespace nx;                                                              \
            ::nx::detail::build_test(__FILE__, __LINE__, #function, &function, __VA_ARGS__); \
        }                                                                                    \
    } _nx_obj_register##function;                                                            \
    }                                                                                        \
    static void function()

namespace nx
{
/// returns the seed to be used for the current test
/// NOTE: only valid inside a test!
NX_API size_t get_seed();

namespace detail
{
NX_API Test* register_test(char const* name, char const* file, int line, char const* fun_name, test_fun_t fun);
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

template <class... Args>
void build_test(char const* file, int line, char const* fun_name, test_fun_t fun, char const* name, Args&&... args)
{
    [[maybe_unused]] auto t = register_test(name, file, line, fun_name, fun);
    ((configure(t, args)), ...);
}
}
}
