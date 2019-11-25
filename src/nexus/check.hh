#pragma once

#include <clean-core/always_false.hh>
#include <clean-core/forward.hh>
#include <clean-core/has_operator.hh>
#include <clean-core/macros.hh>

#ifdef NX_HAS_REFLECTOR
// TODO: this could be removed by some kind of manual to_string
#include <reflector/to_string.hh>
#endif

#ifndef NX_FORCE_MACRO_PREFIX

#define CHECK(...) NX_CHECK(__VA_ARGS__)
#define REQUIRE(...) NX_REQUIRE(__VA_ARGS__)

#endif

#define NX_CHECK(...) NX_IMPL_CHECK(false, __VA_ARGS__)
#define NX_REQUIRE(...) NX_IMPL_CHECK(true, __VA_ARGS__)


// ================= Implementation =================

#define NX_IMPL_CHECK(terminate, ...)                                                                          \
    do                                                                                                         \
    {                                                                                                          \
        ::nx::detail::number_of_assertions()++;                                                                \
        ::nx::detail::check_result r = ::nx::detail::start_check{} < __VA_ARGS__;                              \
        if (!r.is_true)                                                                                        \
        {                                                                                                      \
            ::nx::detail::number_of_failed_assertions()++;                                                     \
            ::nx::detail::report_failed_check(r, #__VA_ARGS__, __FILE__, __LINE__, CC_PRETTY_FUNC, terminate); \
        }                                                                                                      \
    } while (0)

#define NX_IMPL_FORBID_COMPLEX_CHAIN                                                                                                   \
    template <class R>                                                                                                                 \
    check_result operator&&(R&&) const                                                                                                 \
    {                                                                                                                                  \
        static_assert(cc::always_false<R>, "expression too complex. please just use multiple CHECK()/REQUIRE() statements.");          \
        return {};                                                                                                                     \
    }                                                                                                                                  \
    template <class R>                                                                                                                 \
    check_result operator||(R&&) const                                                                                                 \
    {                                                                                                                                  \
        static_assert(cc::always_false<R>, "expression too complex. a || b is not supported. you can use CHECK((a || b)) if needed."); \
        return {};                                                                                                                     \
    }                                                                                                                                  \
    CC_FORCE_SEMICOLON

namespace nx::detail
{
inline int& number_of_assertions()
{
    thread_local static int cnt = 0;
    return cnt;
}
inline int& number_of_failed_assertions()
{
    thread_local static int cnt = 0;
    return cnt;
}

template <class T>
cc::string make_string_repr(T const& v)
{
    cc::string s;
    // TODO: some basic to_string in case reflector is not available
#ifdef NX_HAS_REFLECTOR
    if constexpr (rf::has_to_string<T>)
        s = rf::to_string(v);
    else
#endif
        s = "???";
    return s;
}

struct check_result
{
    bool is_true = false;
    cc::string lhs;
    cc::string rhs;
    char const* op = nullptr;

    check_result() = default;

    template <class A, class B>
    check_result(bool is_true, A const& a, B const& b, char const* op) : is_true(is_true), op(op)
    {
        if (!is_true)
        {
            lhs = ::nx::detail::make_string_repr(a);
            rhs = ::nx::detail::make_string_repr(b);
        }
    }
    template <class A>
    check_result(bool is_true, A const& a) : is_true(is_true)
    {
        if (!is_true)
            lhs = ::nx::detail::make_string_repr(a);
    }

    NX_IMPL_FORBID_COMPLEX_CHAIN;
};

template <class T>
struct checker
{
    T value;

    template <class R>
    check_result operator<(R&& rhs) const
    {
        static_assert(cc::has_operator_equal<T, R>, "operator< is not defined for the arguments");
        return {bool(value < rhs), value, rhs, "<"};
    }
    template <class R>
    check_result operator<=(R&& rhs) const
    {
        static_assert(cc::has_operator_equal<T, R>, "operator<= is not defined for the arguments");
        return {bool(value <= rhs), value, rhs, "<="};
    }
    template <class R>
    check_result operator>(R&& rhs) const
    {
        static_assert(cc::has_operator_equal<T, R>, "operator> is not defined for the arguments");
        return {bool(value > rhs), value, rhs, ">"};
    }
    template <class R>
    check_result operator>=(R&& rhs) const
    {
        static_assert(cc::has_operator_equal<T, R>, "operator>= is not defined for the arguments");
        return {bool(value >= rhs), value, rhs, ">="};
    }
    template <class R>
    check_result operator==(R&& rhs) const
    {
        static_assert(cc::has_operator_equal<T, R>, "operator== is not defined for the arguments");
        return {bool(value == rhs), value, rhs, "=="};
    }
    template <class R>
    check_result operator!=(R&& rhs) const
    {
        static_assert(cc::has_operator_not_equal<T, R>, "operator!= is not defined for the arguments");
        return {bool(value != rhs), value, rhs, "!="};
    }

    operator check_result() const
    {
        static_assert(std::is_constructible_v<bool, T>, "cannot convert argument to bool");
        return {bool(value), value};
    }

    NX_IMPL_FORBID_COMPLEX_CHAIN;
};

// TODO: verify that checker does not copy unnecessarily
struct start_check
{
    template <class T>
    checker<T> operator<(T&& v) const
    {
        return {cc::forward<T>(v)};
    }
};

void report_failed_check(check_result const& r, char const* check, char const* file, int line, char const* function, bool terminate);
}
