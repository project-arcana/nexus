#pragma once

#include <nexus/fwd.hh>

#include <clean-core/string.hh>

namespace nx
{
/**
 * Represents a single app
 */
class App
{
    // properties
public:
    cc::string const& name() const { return mName; }
    cc::string const& file() const { return mFile; }
    int line() const { return mLine; }
    cc::string const& functionName() const { return mFunctionName; }
    app_fun_t function() const { return mFunction; }
    int argc() const{return mArgC;}
    char** argv() const{return mArgV;}

    // ctor
public:
    App(char const* name, char const* file, int line, char const* fun_name, app_fun_t fun);

    App(App const&) = delete;
    App(App&&) = delete;
    App& operator=(App const&) = delete;
    App& operator=(App&&) = delete;

    // members
private:
    cc::string mName;
    cc::string mFile;
    int mLine;
    cc::string mFunctionName;
    app_fun_t mFunction;
    int mArgC = 0;
    char** mArgV = nullptr;

    friend class Nexus;
};

namespace detail
{
cc::vector<cc::unique_ptr<App>>& get_all_apps();
App* get_current_app(); // TODO: discuss if and how this should be open API
}
}
