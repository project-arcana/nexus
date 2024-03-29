#pragma once

#include <clean-core/string.hh>

#ifdef NX_HAS_REFLECTOR
// TODO: this could be removed by some kind of manual to_string
#include <reflector/to_string.hh>
#endif

namespace nx::detail
{
template <class T>
cc::string make_string_repr(T const& v)
{
    cc::string s;
    if constexpr (std::is_same_v<T, decltype(nullptr)>)
        s = "[nullptr]";
    else if constexpr (std::is_constructible_v<cc::string, T const&>)
        s = '"' + cc::string(v) + '"';
#ifdef NX_HAS_REFLECTOR
    // TODO: some basic to_string in case reflector is not available
    else if constexpr (rf::has_to_string<T>)
        s = rf::to_string(v);
    else
#endif
        s = "???";
    return s;
}
}
