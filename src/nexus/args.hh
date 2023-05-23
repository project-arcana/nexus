#pragma once

#include <clean-core/function_ptr.hh>
#include <clean-core/span.hh>
#include <clean-core/string.hh>
#include <clean-core/string_view.hh>
#include <clean-core/unique_function.hh>
#include <clean-core/vector.hh>

#include <nexus/detail/api.hh>
#include <nexus/detail/parse_arg.hh>

namespace nx
{
/// easy parsing of command line args for Nexus Apps
///
/// Supports almost all features desired in
/// https://attractivechaos.wordpress.com/2018/08/31/a-survey-of-argument-parsing-libraries-in-c-c/
///
/// TODO:
///   - generic deserialization
///   - parse(string_view) version
///   - define positional args
///   - get with optional return type
struct NX_API args
{
    // setup
public:
    args();
    args(cc::string app_name, cc::string app_desc = "");

    args(args&&) = default;
    args(args const&) = delete;
    args& operator=(args&&) = default;
    args& operator=(args const&) = delete;

    args& disable_help() &;
    args&& disable_help() &&;

    args& group(cc::string name) &;
    args&& group(cc::string name) &&;

    template <class T>
    args& add(T& v, cc::string name, cc::string desc) &
    {
        auto& a = add_arg({cc::move(name)}, cc::move(desc));
        a.register_var_parse(v);
        return *this;
    }

    template <class T>
    args&& add(T& v, cc::string name, cc::string desc) &&
    {
        auto& a = add_arg({cc::move(name)}, cc::move(desc));
        a.register_var_parse(v);
        return cc::move(*this);
    }

    template <class T>
    args& add(T& v, std::initializer_list<cc::string> names, cc::string desc) &
    {
        auto& a = add_arg(names, cc::move(desc));
        a.register_var_parse(v);
        return *this;
    }
    template <class T>
    args&& add(T& v, std::initializer_list<cc::string> names, cc::string desc) &&
    {
        auto& a = add_arg(names, cc::move(desc));
        a.register_var_parse(v);
        return cc::move(*this);
    }

    template <class T = bool>
    args& add(cc::string name, cc::string desc) &
    {
        auto& a = add_arg({cc::move(name)}, cc::move(desc));
        a.expect_val = !std::is_same_v<T, bool>;
        return *this;
    }
    template <class T = bool>
    args&& add(cc::string name, cc::string desc) &&
    {
        auto& a = add_arg({cc::move(name)}, cc::move(desc));
        a.expect_val = !std::is_same_v<T, bool>;
        return cc::move(*this);
    }

    template <class T = bool>
    args& add(std::initializer_list<cc::string> names, cc::string desc) &
    {
        auto& a = add_arg(names, cc::move(desc));
        a.expect_val = !std::is_same_v<T, bool>;
        return *this;
    }
    template <class T = bool>
    args&& add(std::initializer_list<cc::string> names, cc::string desc) &&
    {
        auto& a = add_arg(names, cc::move(desc));
        a.expect_val = !std::is_same_v<T, bool>;
        return cc::move(*this);
    }

    args& version(cc::string v) &;
    args&& version(cc::string v) &&;

    template <class T>
    args& add_positional(T& v, char metavar, cc::string desc) &
    {
        auto& a = add_pos_arg(metavar, cc::move(desc));
        a.register_var_parse(v);
        return *this;
    }
    template <class T>
    args&& add_positional(T& v, char metavar, cc::string desc) &&
    {
        auto& a = add_pos_arg(metavar, cc::move(desc));
        a.register_var_parse(v);
        return cc::move(*this);
    }
    template <class T>
    args& add_positional(char metavar, cc::string desc) &
    {
        add_pos_arg(metavar, cc::move(desc));
        return *this;
    }
    template <class T>
    args&& add_positional(char metavar, cc::string desc) &&
    {
        add_pos_arg(metavar, cc::move(desc));
        return cc::move(*this);
    }
    template <class T>
    args& add_positional_variadic(char metavar, cc::string desc) &
    {
        auto& a = add_pos_arg(metavar, cc::move(desc));
        a.variadic = true;
        return *this;
    }
    template <class T>
    args&& add_positional_variadic(char metavar, cc::string desc) &&
    {
        auto& a = add_pos_arg(metavar, cc::move(desc));
        a.variadic = true;
        return cc::move(*this);
    }

    /// adds a function to be called at the end of parsing to validate an arbitrary property
    /// if validate_fun returns false, desc is printed and parsing returns false
    ///
    /// usage example:
    ///
    ///   int i, j;
    ///   auto args = nx::args(...);
    ///   args.add(i, ...) //
    ///       .add(j, ...)
    ///       ...
    ///       .validate("i and j must have same sign", [&] { return i * j > 0; });
    ///
    ///   if (!args.parse(...))
    ///   {
    ///       args.print_help();
    ///       return;
    ///   }
    ///
    ///   CC_ASSERT(i * j > 0); // will never fail here
    ///
    args& validate(cc::string_view desc, cc::unique_function<bool()> validate_fun) &;
    args&& validate(cc::string_view desc, cc::unique_function<bool()> validate_fun) &&;

    // parse
public:
    /// takes app args
    bool parse();
    /// takes space-separated args
    /// NOTE: does currently NOT respect quotes, though that will change in the future
    bool parse(cc::string_view args);
    /// NOTE: does NOT expect the binary filename as first arg, use parse_main for that
    bool parse(int argc, char const* const* argv);
    /// same as parse, but ignores the first argument
    bool parse_main(int argc, char const* const* argv);
    void print_help() const;

    // retrieval
public:
    bool has(cc::string_view name) const;
    int count_of(cc::string_view name) const;
    int idx_of(cc::string_view name) const; // -1 if not found

    template <class T>
    T get_or(cc::string_view name, T const& def) const
    {
        for (auto const& a : _parsed_args)
            for (auto const& n : a.a->names)
                if (n == name)
                {
                    T v;
                    static_assert(!std::is_same_v<decltype(nx::detail::parse_arg(v, a.value)), nx::detail::not_supported>, //
                                  "argument type not supported");
                    if (nx::detail::parse_arg(v, a.value))
                        return v;
                    else
                        return def;
                }
        return def;
    }

    template <class T>
    cc::vector<T> get_all(cc::string_view name) const
    {
        cc::vector<T> res;
        for (auto const& a : _parsed_args)
            for (auto const& n : a.a->names)
                if (n == name)
                {
                    T v;
                    static_assert(!std::is_same_v<decltype(nx::detail::parse_arg(v, a.value)), nx::detail::not_supported>, //
                                  "argument type not supported");

                    // TODO: report error case?
                    if (nx::detail::parse_arg(v, a.value))
                        res.push_back(cc::move(v));
                }
        return res;
    }

    cc::vector<cc::string> positional_args() const;

private:
    struct arg
    {
        bool expect_val = true;
        cc::vector<cc::string> names;
        cc::string description;
        cc::string group;
        void* target = nullptr;
        cc::function_ptr<bool(void*, cc::string const&)> on_parse = nullptr;

        template <class T>
        void register_var_parse(T& v)
        {
            target = &v;

            if constexpr (std::is_same_v<T, bool>)
            {
                expect_val = false;
                on_parse = [](void* t, cc::string const&) -> bool
                {
                    *static_cast<bool*>(t) = true;
                    return true;
                };
            }
            else
            {
                expect_val = true;
                on_parse = [](void* t, cc::string const& s) -> bool
                {
                    T& vv = *static_cast<T*>(t);
                    static_assert(!std::is_same_v<decltype(nx::detail::parse_arg(vv, s)), nx::detail::not_supported>, "argument type not supported");
                    return nx::detail::parse_arg(vv, s);
                };
            }
        }
    };

    struct pos_arg
    {
        int min_count = 0;
        char metavar = 0;
        cc::string description;
        bool variadic = false; // only supported for the last one
        void* target = nullptr;
        cc::function_ptr<bool(void*, cc::string const&)> on_parse = nullptr;

        template <class T>
        void register_var_parse(T& v)
        {
            target = &v;

            on_parse = [](void* t, cc::string const& s) -> bool
            {
                T& vv = *static_cast<T*>(t);
                static_assert(!std::is_same_v<decltype(nx::detail::parse_arg(vv, s)), nx::detail::not_supported>, "argument type not supported");
                return nx::detail::parse_arg(vv, s);
            };
        }
    };

    struct parsed_arg
    {
        arg const* a = nullptr;
        cc::string value;
    };

    struct parsed_pos_arg
    {
        pos_arg const* a = nullptr;
        cc::vector<cc::string> values;
    };

    struct validator
    {
        cc::string desc;
        cc::unique_function<bool()> fun;
    };

    arg& add_arg(std::initializer_list<cc::string> names, cc::string desc);
    pos_arg& add_pos_arg(char n, cc::string desc);

    bool _disable_help = false;
    cc::string _version;
    cc::string _app_name;
    cc::string _app_desc;
    cc::string _curr_group;

    cc::vector<arg> _args;
    cc::vector<pos_arg> _pos_args;

    cc::vector<parsed_arg> _parsed_args;
    cc::vector<parsed_pos_arg> _parsed_pos_args;

    cc::vector<validator> _validators;
};

/// returns all command line arguments of the currently running test or app
NX_API cc::span<char const* const> get_cmd_args();

/// returns true if the current test or app was launched with the given command line argument
NX_API bool has_cmd_arg(cc::string_view arg);

/// returns the value of the given argument on the command line (or an empty string view)
/// requires syntax like --<flag> <value>
/// where <value> does not begin with '-'
NX_API cc::string_view get_cmd_arg_value(cc::string_view arg);
}
