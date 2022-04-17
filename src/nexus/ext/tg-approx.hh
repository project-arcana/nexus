#pragma once

#include <clean-core/assert.hh>
#include <clean-core/enable_if.hh>

#include <typed-geometry/tg-lean.hh>

#include <nexus/approx.hh>
#include <nexus/detail/make_string_repr.hh>

#include <cmath>

namespace nx
{
namespace detail
{
template <class T, int D, class ScalarT>
struct tg_comp_abs_approx
{
    tg_comp_abs_approx(T const& v) : _value(v) {}

    T const& value() const { return _value; }

    ScalarT get_abs() const { return _eps_abs; }
    ScalarT get_rel() const { return _eps_rel; }

    tg_comp_abs_approx abs(ScalarT const& e) const
    {
        auto a = *this;
        a._eps_abs = e;
        return a;
    }

    tg_comp_abs_approx rel(ScalarT const& e) const
    {
        auto a = *this;
        a._eps_rel = e;
        return a;
    }

    bool operator==(T const& rhs) const
    {
        for (auto i = 0; i < D; ++i)
            if (abs_approx<ScalarT>(_value[i]).abs(_eps_abs).rel(_eps_rel) != rhs[i])
                return false;
        return true;
    }
    bool operator!=(T const& rhs) const { return !operator==(rhs); }

private:
    T _value;
    ScalarT _eps_abs = default_abs_epsilon<ScalarT>::value;
    ScalarT _eps_rel = default_rel_epsilon<ScalarT>::value;
};

template <class T, int C, int R, class ScalarT>
struct tg_mat_abs_approx
{
    tg_mat_abs_approx(T const& v) : _value(v) {}

    T const& value() const { return _value; }

    ScalarT get_abs() const { return _eps_abs; }
    ScalarT get_rel() const { return _eps_rel; }

    tg_mat_abs_approx abs(ScalarT const& e) const
    {
        auto a = *this;
        a._eps_abs = e;
        return a;
    }

    tg_mat_abs_approx rel(ScalarT const& e) const
    {
        auto a = *this;
        a._eps_rel = e;
        return a;
    }

    bool operator==(T const& rhs) const
    {
        for (auto x = 0; x < C; ++x)
            for (auto y = 0; y < C; ++y)
                if (abs_approx<ScalarT>(_value[x][y]).abs(_eps_abs).rel(_eps_rel) != rhs[x][y])
                    return false;
        return true;
    }
    bool operator!=(T const& rhs) const { return !operator==(rhs); }

private:
    T _value;
    ScalarT _eps_abs = default_abs_epsilon<ScalarT>::value;
    ScalarT _eps_rel = default_rel_epsilon<ScalarT>::value;
};

template <class T, int D, class ScalarT>
bool operator==(T const& lhs, tg_comp_abs_approx<T, D, ScalarT> const& rhs)
{
    return rhs.operator==(lhs);
}
template <class T, int D, class ScalarT>
bool operator!=(T const& lhs, tg_comp_abs_approx<T, D, ScalarT> const& rhs)
{
    return rhs.operator!=(lhs);
}

template <class T, int C, int R, class ScalarT>
bool operator==(T const& lhs, tg_mat_abs_approx<T, C, R, ScalarT> const& rhs)
{
    return rhs.operator==(lhs);
}
template <class T, int C, int R, class ScalarT>
bool operator!=(T const& lhs, tg_mat_abs_approx<T, C, R, ScalarT> const& rhs)
{
    return rhs.operator!=(lhs);
}

template <class T, int D, class ScalarT>
cc::string to_string(tg_comp_abs_approx<T, D, ScalarT> const& v)
{
    cc::string s = "approx (";
    for (auto i = 0; i < D; ++i)
    {
        if (i > 0)
            s += ", ";
        s += nx::detail::make_string_repr(v.value()[i]);
    }
    s += ")";
    if (v.get_abs() != default_abs_epsilon<ScalarT>::value || v.get_rel() != default_rel_epsilon<ScalarT>::value)
    {
        s += " (abs: ";
        s += nx::detail::make_string_repr(v.get_abs());
        s += ", rel: ";
        s += nx::detail::make_string_repr(v.get_rel());
        s += ")";
    }
    return s;
}

template <class T, int C, int R, class ScalarT>
cc::string to_string(tg_mat_abs_approx<T, C, R, ScalarT> const& v)
{
    cc::string s = "approx [";
    for (auto y = 0; y < R; ++y)
    {
        if (y > 0)
            s += ", ";
        s += "(";
        for (auto x = 0; x < C; ++x)
        {
            if (x > 0)
                s += ", ";
            s += nx::detail::make_string_repr(v.value()[x][y]);
        }
        s += ")";
    }
    s += "]";
    if (v.get_abs() != default_abs_epsilon<ScalarT>::value || v.get_rel() != default_rel_epsilon<ScalarT>::value)
    {
        s += " (abs: ";
        s += nx::detail::make_string_repr(v.get_abs());
        s += ", rel: ";
        s += nx::detail::make_string_repr(v.get_rel());
        s += ")";
    }
    return s;
}

template <class T>
struct tg_angle_approx
{
    using angle_t = tg::angle_t<T>;

    tg_angle_approx(angle_t const& v) : _value(v) {}

    angle_t const& value() const { return _value; }

    tg_angle_approx abs(angle_t const& e) const
    {
        auto a = *this;
        a._eps_abs = e;
        return a;
    }

    angle_t get_abs() const { return _eps_abs; }

    template <class U>
    bool operator==(U const& rhs) const
    {
        static_assert(std::is_same_v<angle_t, U>, "types must match exactly to prevent errors");

        auto diff = _value.radians() - rhs.radians();
        if (diff < 0)
            diff = -diff;
        diff = std::fmod(diff, tg::pi_scalar<T>);

        return diff < _eps_abs.radians();
    }
    template <class U>
    bool operator!=(U const& rhs) const
    {
        return !operator==(rhs);
    }

private:
    angle_t _value;
    angle_t _eps_abs = angle_t::from_radians(default_abs_epsilon<T>::value);
};

template <class T, class U>
bool operator==(U const& lhs, tg_angle_approx<T> const& rhs)
{
    return rhs.operator==(lhs);
}
template <class T, class U>
bool operator!=(U const& lhs, tg_angle_approx<T> const& rhs)
{
    return rhs.operator!=(lhs);
}

template <class T>
cc::string to_string(tg_angle_approx<T> const& v)
{
    auto s = "approx " + nx::detail::make_string_repr(v.value().degree()) + "°";
    if (v.get_abs().radians() != default_abs_epsilon<T>::value)
    {
        s += " (abs: ";
        s += nx::detail::make_string_repr(v.get_abs().radians());
        s += "°)";
    }
    return s;
}
}

template <class ScalarT, int D>
detail::tg_comp_abs_approx<tg::vec<D, ScalarT>, D, ScalarT> approx(tg::vec<D, ScalarT> const& v)
{
    return {v};
}
template <class ScalarT, int D>
detail::tg_comp_abs_approx<tg::pos<D, ScalarT>, D, ScalarT> approx(tg::pos<D, ScalarT> const& v)
{
    return {v};
}
template <class ScalarT, int D>
detail::tg_comp_abs_approx<tg::dir<D, ScalarT>, D, ScalarT> approx(tg::dir<D, ScalarT> const& v)
{
    return {v};
}
template <class ScalarT, int D>
detail::tg_comp_abs_approx<tg::comp<D, ScalarT>, D, ScalarT> approx(tg::comp<D, ScalarT> const& v)
{
    return {v};
}
template <class ScalarT, int D>
detail::tg_comp_abs_approx<tg::color<D, ScalarT>, D, ScalarT> approx(tg::color<D, ScalarT> const& v)
{
    return {v};
}
template <class ScalarT, int D>
detail::tg_comp_abs_approx<tg::size<D, ScalarT>, D, ScalarT> approx(tg::size<D, ScalarT> const& v)
{
    return {v};
}
template <class ScalarT>
detail::tg_comp_abs_approx<tg::quaternion<ScalarT>, 4, ScalarT> approx(tg::quaternion<ScalarT> const& v)
{
    return {v};
}
template <class ScalarT, int C, int R>
detail::tg_mat_abs_approx<tg::mat<C, R, ScalarT>, C, R, ScalarT> approx(tg::mat<C, R, ScalarT> const& v)
{
    return {v};
}
template <class T>
detail::tg_angle_approx<T> approx(tg::angle_t<T> const& v)
{
    return {v};
}
}
