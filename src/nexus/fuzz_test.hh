#pragma once

#include "test.hh"

#include <typed-geometry/feature/random.hh>

#ifndef NX_FORCE_MACRO_PREFIX

#define FUZZ_TEST(...) NX_FUZZ_TEST(__VA_ARGS__)

#endif

/**
 * Defines a fuzz test
 *
 * Usage:
 *   FUZZ_TEST("some test")(tg::rng& rng)
 *   {
 *       auto i = uniform(rng, 0, 10);
 *       CHECK(i >= 0);
 *   }
 */
#define NX_FUZZ_TEST(...) NX_DETAIL_REGISTER_FUZZ_TEST(CC_MACRO_JOIN(_nx_anon_fuzz_test_function_, __COUNTER__), __VA_ARGS__)

#define NX_DETAIL_REGISTER_FUZZ_TEST(fuzz_fun, ...)                     \
    static void fuzz_fun(tg::rng&);                                            \
    NX_TEST(__VA_ARGS__) { ::nx::detail::execute_fuzz_test(fuzz_fun); } \
    void fuzz_fun

namespace nx::detail
{
void execute_fuzz_test(void (*f)(tg::rng&));
}
