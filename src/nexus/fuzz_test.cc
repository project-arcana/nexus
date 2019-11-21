#include "fuzz_test.hh"

#include <ctracer/trace.hh>

#include <nexus/tests/Test.hh>

void nx::detail::execute_fuzz_test(void (*f)(tg::rng&))
{
    // TODO: take config from: auto test = get_current_test();

    auto const max_cycles = 10'000'000; // TODO: configurable
    auto const max_iterations = 1000;   // TODO: configurable

    tg::rng base_rng; // TODO: seed
    auto c_start = ct::current_cycles();
    for (auto i = 0; i < max_iterations; ++i)
    {
        auto seed = base_rng();
        tg::rng rng;
        rng.seed(seed);

        // TODO: check if this test failed and report the seed
        f(rng);

        if (ct::current_cycles() - c_start > max_cycles)
            break;
    }
}
