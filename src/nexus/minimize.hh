#pragma once

#include <nexus/minimize_options.hh>

namespace nx
{
/// generic minimization of a test case
/// takes an input test case (any type)
///       an option generator (function from T -> minimize_options<T>)
///       an is_failing predicate (function from T -> bool)
/// returns a minimized version of T that is failing (if the input is failing)
///
/// NOTE: options must not generate cycles!
///       it is understood that each provided option is "smaller" in the user-desired sense
///       and "smaller" is well-founded
///
/// NOTE: the functions can also accept T const&
///       this arg is guaranteed to surpass the lifetime of the returning minimize_options<T>
template <class T, class OptionGeneratorF, class IsFailingF>
T minimize(T input, OptionGeneratorF&& generate_options, IsFailingF&& is_failing)
{
    static_assert(std::is_invocable_r_v<minimize_options<T>, OptionGeneratorF, T>);
    static_assert(std::is_invocable_r_v<bool, IsFailingF, T>);

    while (true)
    {
        minimize_options<T> opts = generate_options(input);

        auto has_smaller = false;
        while (opts.has_options_left())
        {
            T smaller_input = opts.get_next_option_for(input);
            if (is_failing(smaller_input))
            {
                input = cc::move(smaller_input);
                has_smaller = true;
                break;
            }
        }

        if (!has_smaller)
            break;
    }

    return input;
}
}
