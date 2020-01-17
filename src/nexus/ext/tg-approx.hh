#pragma once

#include <clean-core/assert.hh>
#include <clean-core/enable_if.hh>

#include <typed-geometry/tg-lean.hh>

#include <nexus/approx.hh>
#include <nexus/detail/make_string_repr.hh>

namespace nx
{
namespace detail
{
template <class T, int D, class ScalarT>
struct tg_comp_abs_approx
{
    tg_comp_abs_approx(T const& v) : _value(v) {}

    T const& value() const { return _value; }

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
            if (abs_approx<ScalarT>(_value[i]) != rhs[i])
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
                if (abs_approx<ScalarT>(_value[x][y]) != rhs[x][y])
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
    return s + ")";
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
    return s + "]";
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
template <class ScalarT, int C, int R>
detail::tg_mat_abs_approx<tg::mat<C, R, ScalarT>, C, R, ScalarT> approx(tg::mat<C, R, ScalarT> const& v)
{
    return {v};
}
}
