#pragma once

#include <clean-core/macros.hh>

#include <nexus/args.hh>
#include <nexus/detail/api.hh>

#include "fwd.hh"

#ifndef NX_FORCE_MACRO_PREFIX

#define APP(...) NX_APP(__VA_ARGS__)

#endif

/**
 * defines a new Nexus App
 *
 * TEST("myapp")
 * {
 *    .. do something
 * }
 *
 * Can be called by passing the name as the first arg `~ myapp`
 */
#define NX_APP(...) NX_DETAIL_REGISTER_APP(CC_MACRO_JOIN(_nx_app_function_, __COUNTER__), __VA_ARGS__)

// second layer to make sure function is expanded
#define NX_DETAIL_REGISTER_APP(function, ...) NX_DETAIL_REGISTER_APP2(function, __VA_ARGS__)

#define NX_DETAIL_REGISTER_APP2(function, ...)                                              \
    static void function();                                                                 \
    namespace                                                                               \
    {                                                                                       \
    struct _nx_register##function                                                           \
    {                                                                                       \
        _nx_register##function()                                                            \
        {                                                                                   \
            using namespace nx;                                                             \
            ::nx::detail::build_app(__FILE__, __LINE__, #function, &function, __VA_ARGS__); \
        }                                                                                   \
    } _nx_obj_register##function;                                                           \
    }                                                                                       \
    static void function()

namespace nx::detail
{
NX_API App* register_app(char const* name, char const* file, int line, char const* fun_name, app_fun_t fun);

template <class... Args>
void build_app(char const* file, int line, char const* fun_name, app_fun_t fun, char const* name, Args&&... args)
{
    [[maybe_unused]] auto t = register_app(name, file, line, fun_name, fun);
    ((configure(t, args)), ...);
}
}
