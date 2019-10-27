#pragma once

#include "check.hh"
#include "config.hh"
#include "fwd.hh"

#include <clean-core/macros.hh>

#ifndef NX_FORCE_MACRO_PREFIX

#define APP(...) NX_APP(__VA_ARGS__)
#define TEST(...) NX_TEST(__VA_ARGS__)
#define BENCHMARK(...) NX_BENCHMARK(__VA_ARGS__)

#endif

/**
 * defines a test case
 *
 * - first argument is a name, rest are optional configurations
 * - the same name may be reused many times
 *
 * Examples:
 *  NX_TEST("my test")
 *  {
 *      CHECK(1 + 1 == 2);
 *  }
 *
 *  // is not run concurrently with any other test
 *  NX_TEST("my test", exclusive) { ... }
 *
 *  // runs this test before/after some other tests (specified via pattern)
 *  // if the other tests fail for some reason, this test is not run at all
 *  NX_TEST("my test", before("other test"), after("some pattern*")) { ... }
 */
#define NX_TEST(...) NX_DETAIL_REGISTER_TEST(CC_MACRO_JOIN(_nx_anon_test_function_, __COUNTER__), __VA_ARGS__)

// second layer to make sure function is expanded
#define NX_DETAIL_REGISTER_TEST(function, ...) NX_DETAIL_REGISTER_TEST2(function, __VA_ARGS__)

#define NX_DETAIL_REGISTER_TEST2(function, ...)                                   \
    static void function();                                                       \
    namespace                                                                     \
    {                                                                             \
    struct _nx_register##function                                                 \
    {                                                                             \
        _nx_register##function()                                                  \
        {                                                                         \
            using namespace nx;                                                   \
            ::nx::detail::build_test(__FILE__, __LINE__, &function, __VA_ARGS__); \
        }                                                                         \
    } _nx_obj_register##function;                                                 \
    }                                                                             \
    static void function()

namespace nx
{
namespace detail
{
Test* register_test(char const* name, char const* file, int line, test_fun_t fun);
void configure(Test* t, before const& v);
void configure(Test* t, after const& v);
void configure(Test* t, exclusive_t const&);

template <class... Args>
void build_test(char const* file, int line, test_fun_t fun, char const* name, Args&&... args)
{
    auto t = register_test(name, file, line, fun);
    ((configure(t, args)), ...);
}
}
}
