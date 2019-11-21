#pragma once

#include "test.hh"
#include "tests/MonteCarloTest.hh"

#include <typed-geometry/feature/random.hh>

#ifndef NX_FORCE_MACRO_PREFIX

#define MONTE_CARLO_TEST(...) NX_MONTE_CARLO_TEST(__VA_ARGS__)

#endif

/**
 * Defines a monte carlo test
 *
 * NOTE: monte carlo tests have a builtin tg::rng called "built-in rng"
 *
 * Usage:
 *   MONTE_CARLO_TEST("some test")
 *   {
 *      addOp("gen", [](tg::rng& rng) { return uniform(rng, -10, 10) * 2; }
 *      addOp("add", [](int a, int b) { return a + b; });
 *      addOp("sub", [](int a, int b) { return a - b; });
 *      addInvariant("mod 2", [](int i) { CHECK(i % 2 == 0); });
 *   }
 */
#define NX_MONTE_CARLO_TEST(...) NX_DETAIL_REGISTER_MONTE_CARLO_TEST(CC_MACRO_JOIN(_nx_monte_carlo_test_, __COUNTER__), __VA_ARGS__)

#define NX_DETAIL_REGISTER_MONTE_CARLO_TEST(mct_class, ...) \
    namespace                                               \
    {                                                       \
    struct mct_class : ::nx::MonteCarloTest                 \
    {                                                       \
        mct_class();                                        \
    };                                                      \
    }                                                       \
    NX_TEST(__VA_ARGS__)                                    \
    {                                                       \
        mct_class mct;                                      \
        mct.addValue("built-in rng", tg::rng());            \
        mct.execute();                                      \
    }                                                       \
    mct_class::mct_class()
