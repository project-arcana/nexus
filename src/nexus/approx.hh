#pragma once

#include <clean-core/assert.hh>
#include <clean-core/enable_if.hh>

#include <nexus/detail/make_string_repr.hh>

namespace nx
{
namespace detail
{
template <class T>
struct default_abs_epsilon
{
    static constexpr T value = T(0);
};
template <class T>
struct default_rel_epsilon
{
    static constexpr T value = T(1e-4);
};
template <>
struct default_rel_epsilon<double>
{
    static constexpr double value = 1e-10;
};

template <class T>
struct abs_approx
{
    abs_approx(T const& v) : _value(v) {}

    T const& value() const { return _value; }

    abs_approx abs(T const& e) const
    {
        auto a = *this;
        a._eps_abs = e;
        return a;
    }

    abs_approx rel(T const& e) const
    {
        auto a = *this;
        a._eps_rel = e;
        return a;
    }

    template <class U>
    bool operator==(U const& rhs) const
    {
        static_assert(std::is_same_v<T, U>, "types must match exactly to prevent errors");

        if (rhs > _value)
        {
            auto diff = rhs - _value;

            if (diff <= _eps_abs)
                return true;

            auto div = rhs / _value - 1;

            if (div >= 0 && div <= _eps_rel)
                return true;
        }
        else
        {
            auto diff = _value - rhs;

            if (diff <= _eps_abs)
                return true;

            auto div = _value / rhs - 1;

            if (div >= 0 && div <= _eps_rel)
                return true;
        }

        return false;
    }
    template <class U>
    bool operator!=(U const& rhs) const
    {
        return !operator==(rhs);
    }

private:
    T _value;
    T _eps_abs = default_abs_epsilon<T>::value;
    T _eps_rel = default_rel_epsilon<T>::value;
};

template <class T, class U>
bool operator==(U const& lhs, abs_approx<T> const& rhs)
{
    return rhs.operator==(lhs);
}
template <class T, class U>
bool operator!=(U const& lhs, abs_approx<T> const& rhs)
{
    return rhs.operator!=(lhs);
}

template <class T>
cc::string to_string(abs_approx<T> const& v)
{
    return "approx " + nx::detail::make_string_repr(v.value());
}
}

template <class T, class = std::void_t<decltype(std::declval<T>() < 0)>>
detail::abs_approx<T> approx(T const& v)
{
    return {v};
}
}
