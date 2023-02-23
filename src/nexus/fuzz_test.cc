#include "fuzz_test.hh"

#include <rich-log/log.hh>

#include <chrono>

#include <clean-core/defer.hh>
#include <clean-core/intrinsics.hh>

#include <nexus/detail/assertions.hh>
#include <nexus/detail/exception.hh>
#include <nexus/detail/log.hh>
#include <nexus/tests/Test.hh>


void nx::detail::execute_fuzz_test(void (*f)(tg::rng&))
{
    auto test = get_current_test();

    // reproduction
    if (test->shouldReproduce())
    {
        tg::rng rng;
        rng.seed(test->reproduction().seed);
        f(rng);
        return;
    }

    auto const max_cycles = 10'000'000; // TODO: configurable
    auto const max_iterations = 1000;   // TODO: configurable

    tg::rng base_rng;
    base_rng.seed(test->seed());

    // terminate directly
    nx::detail::always_terminate() = true;
    nx::detail::overwrite_assertion_handlers();
    CC_DEFER
    {
        nx::detail::always_terminate() = false;
        nx::detail::reset_assertion_handlers();
    };

    if (test->isEndless())
        RICH_LOG("endless FUZZ_TEST(\"%s\")", test->name());

    auto c_start = cc::intrin_rdtsc();
    auto it = 0;
    auto assert_cnt_start = nx::detail::number_of_assertions();
    auto t0 = std::chrono::high_resolution_clock::now();
    while (true)
    {
        auto seed = base_rng();
        tg::rng rng;
        rng.seed(seed);

        // execute
        if (test->isDebug())
            f(rng);
        else
        {
            try
            {
                f(rng);
            }
            catch (assertion_failed_exception const&)
            {
                test->setReproduce(reproduce(seed));
                return;
            }
        }
        ++it;

        if (test->isEndless())
        {
            // progress report
            auto t1 = std::chrono::high_resolution_clock::now();
            using namespace std::chrono_literals;
            if (t1 - t0 > 1000ms)
            {
                t0 = t1;
                RICH_LOG("endless FUZZ_TEST: %s assertions", nx::detail::number_of_assertions() - assert_cnt_start);
            }

            continue;
        }

        if (it > max_iterations)
            break;

        if (cc::intrin_rdtsc() - c_start > max_cycles)
            break;
    }
}
