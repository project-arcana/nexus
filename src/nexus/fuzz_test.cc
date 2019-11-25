#include "fuzz_test.hh"

#include <iostream> // TODO: proper log

#include <ctracer/trace.hh>

#include <nexus/detail/assertions.hh>
#include <nexus/detail/exception.hh>
#include <nexus/tests/Test.hh>

#include <clean-core/defer.hh>

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

    auto c_start = ct::current_cycles();
    auto it = 0;
    while (true)
    {
        auto seed = base_rng();
        tg::rng rng;
        rng.seed(seed);

        // TODO: check if this test failed and report the seed
        try
        {
            f(rng);
        }
        catch (assertion_failed_exception const&)
        {
            std::cerr << "[nexus] fuzz test [" << test->name().c_str() << "] failed." << std::endl;
            std::cerr << "        (reproduce via TEST(..., reproduce(" << seed << ")) " << std::endl;
            test->setReproduce(reproduce(seed));
            return;
        }
        ++it;

        if (test->isEndless())
            continue;

        if (it > max_iterations)
            break;

        if (ct::current_cycles() - c_start > max_cycles)
            break;
    }
}
