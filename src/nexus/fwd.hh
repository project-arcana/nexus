#pragma once

namespace nx
{
namespace detail
{
struct local_check_counters;
}

using test_fun_t = void (*)();
using test_fun_before_t = void (*)();
using test_fun_after_t = void (*)(detail::local_check_counters*);
using app_fun_t = void (*)();

class Test;
class App;
class MonteCarloTest;

template <class T>
struct minimize_options;
struct args;
}
