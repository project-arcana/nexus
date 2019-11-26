#pragma once

#include <clean-core/unique_function.hh>
#include <clean-core/vector.hh>

namespace nx
{
/// Sequence of lazy minimization steps
template <class T>
struct minimize_options
{
    cc::vector<cc::unique_function<T(T const&)>> options;
    int position = 0;

    bool has_options_left() const { return position < int(options.size()); }
    T get_next_option_for(T const& v)
    {
        CC_CONTRACT(has_options_left());
        return options[position++](v);
    }
};
}
