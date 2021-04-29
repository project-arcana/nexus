#pragma once

#include <nexus/fwd.hh>

#include <clean-core/span.hh>
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
    char const* name() const { return mName; }
    char const* file() const { return mFile; }
    int line() const { return mLine; }
    char const* functionName() const { return mFunctionName; }
    app_fun_t function() const { return mFunction; }
    int argc() const { return mArgC; }
    char const* const* argv() const { return mArgV; }
    cc::span<char const* const> argSpan() const { return {mArgV, size_t(mArgC)}; }

    // ctor
public:
    App(char const* name, char const* file, int line, char const* fun_name, app_fun_t fun)
      : mName(name), mFile(file), mLine(line), mFunctionName(fun_name), mFunction(fun)
    {
        CC_CONTRACT(name);
    }

    App(App const&) = delete;
    App(App&&) = delete;
    App& operator=(App const&) = delete;
    App& operator=(App&&) = delete;

    // members
private:
    char const* mName;         // name of the app as given in the macro, ie. APP("name") { .. }
    char const* mFile;         // filename where the app is defined, directly from __FILE__ during registration
    int mLine;                 // line where the app is declared in the file, directly from __LINE__
    char const* mFunctionName; // macro-stringified name of the function, ie. "_nx_anon_app_function_5")
    app_fun_t mFunction;       // function pointer to the entry

    int mArgC = 0;
    char const* const* mArgV = nullptr;

    friend class Nexus;
};

namespace detail
{
cc::vector<cc::unique_ptr<App>>& get_all_apps();
App* get_current_app(); // TODO: discuss if and how this should be open API
}
}
