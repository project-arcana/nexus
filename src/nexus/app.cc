#include "app.hh"

#include <nexus/apps/App.hh>

#include <clean-core/unique_ptr.hh>
#include <clean-core/vector.hh>

using namespace nx;

cc::vector<cc::unique_ptr<App>>& nx::detail::get_all_apps()
{
    static cc::vector<cc::unique_ptr<App>> all_apps;
    return all_apps;
}

App* detail::register_app(const char* name, const char* file, int line, char const* fun_name, app_fun_t fun)
{
    auto t = cc::make_unique<App>(name, file, line, fun_name, fun);
    auto t_ptr = t.get();
    get_all_apps().push_back(std::move(t));
    return t_ptr;
}
