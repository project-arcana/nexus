#pragma once

#include <clean-core/forward.hh>

namespace nx::detail
{
template <class T>
struct signature
{
};

// make functor signature

template <class T, class R, class... Args>
signature<R(Args...)> make_fun_signature(R (T::*)(Args...))
{
    return {};
}
template <class T, class R, class... Args>
signature<R(Args...)> make_fun_signature(R (T::*)(Args...) const)
{
    return {};
}
template <class T, class R, class... Args>
signature<R(Args...)> make_fun_signature(R (T::*)(Args...) noexcept)
{
    return {};
}
template <class T, class R, class... Args>
signature<R(Args...)> make_fun_signature(R (T::*)(Args...) const noexcept)
{
    return {};
}


// make signature

template <class R, class... Args>
signature<R(Args...)> make_signature(R (*)(Args...))
{
    return {};
}
template <class R, class... Args>
signature<R(Args...)> make_signature(R (*)(Args...) noexcept)
{
    return {};
}
template <class T, class R>
signature<R(T&)> make_signature(R T::*)
{
    return {};
}
template <class T, class R, class... Args>
signature<R(T&, Args...)> make_signature(R (T::*)(Args...))
{
    return {};
}
template <class T, class R, class... Args>
signature<R(T const&, Args...)> make_signature(R (T::*)(Args...) const)
{
    return {};
}
template <class T, class R, class... Args>
signature<R(T&, Args...)> make_signature(R (T::*)(Args...) noexcept)
{
    return {};
}
template <class T, class R, class... Args>
signature<R(T const&, Args...)> make_signature(R (T::*)(Args...) const noexcept)
{
    return {};
}
template <class F>
auto make_signature(F&&)
{
    return make_fun_signature(&F::operator());
}


// make function

template <class T, class R>
auto make_function(R T::*property)
{
    return [property](T& v) { return v.*property; };
}
template <class T, class R, class... Args>
auto make_function(R (T::*f)(Args...))
{
    return [f](T& v, Args... args) -> R { return (v.*f)(cc::forward<Args>(args)...); };
}
template <class T, class R, class... Args>
auto make_function(R (T::*f)(Args...) const)
{
    return [f](T const& v, Args... args) -> R { return (v.*f)(cc::forward<Args>(args)...); };
}
template <class T, class R, class... Args>
auto make_function(R (T::*f)(Args...) noexcept)
{
    return [f](T& v, Args... args) -> R { return (v.*f)(cc::forward<Args>(args)...); };
}
template <class T, class R, class... Args>
auto make_function(R (T::*f)(Args...) const noexcept)
{
    return [f](T const& v, Args... args) -> R { return (v.*f)(cc::forward<Args>(args)...); };
}
template <class F>
auto make_function(F&& f)
{
    return cc::forward<F>(f);
}

// helper

template <class F, class Op, class R, class... Args>
auto compose(F&& f, Op&& op, signature<R(Args...)>)
{
    return [f = cc::forward<F>(f), op = cc::forward<Op>(op)](Args... args) { return op(f(cc::forward<Args>(args)...)); };
}
}
