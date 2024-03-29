#include "args.hh"

// TODO: remove me (currently used for printing help, could be replaced by cc::print)
#include <iostream>

#include <rich-log/log.hh>

#include <clean-core/pair.hh>
#include <clean-core/set.hh>

#include <nexus/apps/App.hh>
#include <nexus/tests/Test.hh>
#include <nexus/detail/log.hh>

namespace nx
{
args::args()
{
    if (auto const a = nx::detail::get_current_app())
        _app_name = a->name();
}

args::args(cc::string app_name, cc::string app_desc) : _app_name(cc::move(app_name)), _app_desc(cc::move(app_desc)) {}

args& args::disable_help() &
{
    _disable_help = true;
    return *this;
}

args&& args::disable_help() &&
{
    _disable_help = true;
    return cc::move(*this);
}

args& args::group(cc::string name) &
{
    _curr_group = cc::move(name);
    return *this;
}
args&& args::group(cc::string name) &&
{
    _curr_group = cc::move(name);
    return cc::move(*this);
}

args& args::version(cc::string v) &
{
    _version = cc::move(v);
    return *this;
}

args&& args::version(cc::string v) &&
{
    _version = cc::move(v);
    return cc::move(*this);
}

args& args::validate(cc::string_view desc, cc::unique_function<bool()> validate_fun) &
{
    CC_ASSERT(validate_fun.is_valid());
    _validators.push_back({desc, cc::move(validate_fun)});
    return *this;
}

args&& args::validate(cc::string_view desc, cc::unique_function<bool()> validate_fun) &&
{
    CC_ASSERT(validate_fun.is_valid());
    _validators.push_back({desc, cc::move(validate_fun)});
    return cc::move(*this);
}

bool args::parse()
{
    if (auto a = nx::detail::get_current_app())
        return parse(a->argc(), a->argv());

    RICH_LOG_ERROR("no-arg version can only be used inside Nexus Apps" );
    return false;
}

bool args::parse(cc::string_view args)
{
    cc::vector<cc::string> split_args;
    for (auto a : args.split())
        split_args.push_back(a);

    cc::vector<char const*> argv;
    for (auto const& s : split_args)
        argv.push_back(s.c_str());

    return parse(argv.size(), argv.data());
}

bool args::parse(int argc, char const* const* argv)
{
    _parsed_args.clear();
    _parsed_pos_args.clear();

    int pos_idx = 0;
    parsed_arg* valarg = nullptr;
    auto show_help = false;

    auto const add_pos_arg = [&](cc::string_view s)
    {
        // if variadic, just append last one
        if (!_pos_args.empty() && _parsed_pos_args.size() == _pos_args.size() && _pos_args.back().variadic)
        {
            _parsed_pos_args.back().values.push_back(s);
            return true;
        }

        // TODO: proper
        auto& pa = _parsed_pos_args.emplace_back();
        if (pos_idx < int(_pos_args.size()))
            pa.a = &_pos_args[pos_idx];
        pa.values.push_back(s);
        ++pos_idx;
        return true;
    };

    auto const add_long_arg = [&](cc::string_view s)
    {
        if (!_disable_help && s == "help")
        {
            show_help = true;
            return true;
        }

        auto has_value = false;
        cc::string_view v;
        for (size_t i = 0; i < s.size(); ++i)
            if (s[i] == '=')
            {
                has_value = true;
                v = s.subview(i + 1);
                s = s.subview(0, i);
                break;
            }

        for (auto const& a : _args)
            for (auto const& n : a.names)
                if (n == s)
                {
                    auto& pa = _parsed_args.emplace_back();
                    pa.a = &a;
                    if (a.expect_val)
                    {
                        if (has_value)
                            pa.value = v;
                        else
                            valarg = &pa;
                    }
                    else if (has_value)
                    {
                        RICH_LOG_ERROR("argument `--%s' has a value but should not have one.", s );
                        return false;
                    }
                    return true;
                }

        RICH_LOG_ERROR("unknown argument `--%s'", s);
        return false;
    };

    auto const add_short_args = [&](cc::string_view s)
    {
        for (size_t i = 0; i < s.size(); ++i)
        {
            auto c = s.subview(i, 1);

            if (!_disable_help && c == "h")
            {
                show_help = true;
                return true;
            }

            arg const* ar = nullptr;
            for (auto const& a : _args)
                for (auto const& n : a.names)
                    if (n == c)
                    {
                        auto& pa = _parsed_args.emplace_back();
                        pa.a = &a;

                        if (a.expect_val)
                        {
                            if (i != s.size() - 1) // compact
                            {
                                if (s[i + 1] == '=')
                                    pa.value = s.subview(i + 2);
                                else
                                    pa.value = s.subview(i + 1);
                                return true;
                            }
                            else
                            {
                                valarg = &pa;
                                return true;
                            }
                        }

                        ar = &a;
                        break;
                    }

            if (!ar)
            {
                RICH_LOG_ERROR("unknown argument `-%s'", s[i]);
                return false;
            }
        }

        return true;
    };

    for (auto i = 0; i < argc; ++i)
    {
        auto s = cc::string_view(argv[i]);

        if (valarg)
        {
            valarg->value = s;
            valarg = nullptr;
            continue;
        }

        if (s == "--") // rest is positional
        {
            ++i;
            while (i < argc)
            {
                if (!add_pos_arg(argv[i]))
                    return false;
                ++i;
            }
            break;
        }

        // longs args
        if (s.size() > 2 && s.subview(0, 2) == "--")
        {
            if (!add_long_arg(s.subview(2)))
                return false;
        }
        // short args
        else if (s.size() > 1 && s[0] == '-')
        {
            if (!add_short_args(s.subview(1)))
                return false;
        }
        // pos arg
        else
        {
            if (!add_pos_arg(s))
                return false;
        }
    }

    if (valarg)
    {
        RICH_LOG_ERROR("unexpected end of arguments. expected value after `%s'", argv[argc - 1]);
        return false;
    }

    if (show_help)
    {
        print_help();
        return false;
    }

    // check that no value using arg is duplicated
    cc::set<arg const*> blocked_args;
    for (auto const& a : _parsed_args)
    {
        if (!a.a->expect_val)
            continue; // can be duplicated

        if (blocked_args.contains(a.a))
        {
            RICH_LOG_ERROR("argument `%s' was provided multiple times (this is only allowed for boolean/flag args)", a.a->names.front());
            return false;
        }

        blocked_args.add(a.a);
    }

    // convert args
    for (auto& a : _parsed_args)
    {
        if (a.a->target)
        {
            if (!a.a->on_parse(a.a->target, a.value))
                return false;
        }
    }
    for (auto& a : _parsed_pos_args)
    {
        if (a.a && a.a->target)
        {
            CC_ASSERT(a.values.size() == 1 && "variadic pos auto-parse not supported yet");
            if (!a.a->on_parse(a.a->target, a.values[0]))
                return false;
        }
    }

    // validation
    for (auto const& v : _validators)
    {
        if (!v.fun())
        {
            RICH_LOG_ERROR("validation failed: %s", v.desc);
            return false;
        }
    }

    return true;
}

bool args::parse_main(int argc, const char* const* argv) { return parse(argc - 1, argv + 1); }

bool args::has(cc::string_view name) const
{
    for (auto const& a : _parsed_args)
        for (auto const& n : a.a->names)
            if (n == name)
                return true;

    return false;
}

int args::count_of(cc::string_view name) const
{
    auto cnt = 0;

    arg const* fa = nullptr;

    for (auto const& a : _args)
        for (auto const& n : a.names)
            if (n == name)
            {
                fa = &a;
                break;
            }

    if (fa != nullptr)
        for (auto const& a : _parsed_args)
            if (a.a == fa)
                cnt++;

    return cnt;
}

int args::idx_of(cc::string_view name) const
{
    auto idx = 0;

    for (auto const& a : _parsed_args)
    {
        for (auto const& n : a.a->names)
            if (n == name)
                return idx;
        ++idx;
    }

    return -1;
}

cc::vector<cc::string> args::positional_args() const
{
    cc::vector<cc::string> r;
    for (auto const& a : _parsed_pos_args)
        for (auto const& v : a.values)
            r.push_back(v);
    return r;
}

args::arg& args::add_arg(std::initializer_list<cc::string> names, cc::string desc)
{
    CC_ASSERT(names.size() > 0 && "must have at least one name");

    for (auto const& name : names)
    {
        CC_ASSERT(!name.empty() && "cannot have empty arg name");
        CC_ASSERT(name[0] != '-' && "arguments cannot start with dashes");

        for (auto c : name)
        {
            CC_ASSERT(c != ' ' && "arguments cannot contain spaces");
            CC_ASSERT(c != '=' && "arguments cannot contain assignments");
        }

        if (!_disable_help)
        {
            CC_ASSERT(name != "h" && "'h' is reserved for help. call .disable_help() before adding your own -h option.");
            CC_ASSERT(name != "help" && "'help' is reserved for help. call .disable_help() before adding your own --help option.");
        }

        for (auto const& a : _args)
            for (auto const& n : a.names)
                CC_ASSERT(n != name && "duplicate argument name");
    }

    auto& a = _args.emplace_back();
    for (auto const& name : names)
        a.names.push_back(name);
    a.description = cc::move(desc);
    a.group = _curr_group;
    return a;
}

args::pos_arg& args::add_pos_arg(char n, cc::string desc)
{
    for (auto const& a : _pos_args)
        if (n != 0)
            CC_ASSERT(a.metavar != n && "duplicate positional metavar");

    if (!_pos_args.empty() && _pos_args.back().variadic)
        CC_UNREACHABLE("cannot add positional args after a variadic one");

    auto& a = _pos_args.emplace_back();
    a.metavar = n;
    a.description = cc::move(desc);
    return a;
}

void args::print_help() const
{
    if (!_app_name.empty())
        std::cout << _app_name.c_str() << std::endl;
    if (!_app_desc.empty())
    {
        std::cout << std::endl;
        std::cout << _app_desc.c_str() << std::endl;
        std::cout << std::endl;
    }
    if (!_version.empty())
        std::cout << "version: " << _version.c_str() << std::endl;

    struct group_info
    {
        cc::string group;
        size_t max_arg_length = 1;
        cc::vector<cc::pair<cc::string, cc::string>> args;

        void add(cc::string ai, cc::string desc)
        {
            if (ai.size() > max_arg_length)
                max_arg_length = ai.size();
            args.emplace_back(cc::move(ai), cc::move(desc));
        }
    };
    cc::vector<group_info> groupinfos;

    if (!_pos_args.empty())
    {
        groupinfos.emplace_back().group = "positional arguments";
        for (auto const& a : _pos_args)
        {
            auto ai = cc::string("-") + a.metavar;

            groupinfos.back().add(ai, a.description);
        }
    }

    if (!_args.empty() || !_disable_help)
    {
        groupinfos.emplace_back().group = "options";

        if (!_disable_help)
            groupinfos.back().add("-h, --help", "show this help text and exit");

        cc::string_view last_group = "";
        for (auto const& a : _args)
        {
            if (last_group != a.group)
                groupinfos.emplace_back().group = a.group;
            last_group = a.group;

            cc::string ai;
            for (auto const& n : a.names)
            {
                if (!ai.empty())
                    ai += ", ";
                ai += n.size() == 1 ? "-" : "--";
                ai += n;
            }

            groupinfos.back().add(ai, a.description);
        }
    }

    for (auto const& g : groupinfos)
    {
        if (g.args.empty())
            continue;

        std::cout << std::endl;
        std::cout << "[" << g.group.c_str() << "]" << std::endl;

        for (auto const& [a, d] : g.args)
        {
            std::cout << "  " << a.c_str() << cc::string::filled(g.max_arg_length - a.size() + 2, ' ').c_str() << d.c_str() << std::endl;
        }
    }

    if (!_validators.empty())
    {
        std::cout << std::endl;
        std::cout << "[preconditions]" << std::endl;
        for (auto const& v : _validators)
            std::cout << "  - " << v.desc.c_str() << std::endl;
    }

    std::cout << std::endl;
}

cc::span<const char* const> get_cmd_args()
{
    if (Test const* const curr_test = detail::get_current_test())
    {
        return curr_test->argSpan();
    }
    else if (App const* const curr_app = detail::get_current_app())
    {
        return curr_app->argSpan();
    }
    else
    {
        RICH_LOG_WARN("nx::get_cmd_args() was called outside of an active app or test\n");
        return {};
    }
}

bool has_cmd_arg(cc::string_view arg)
{
    for (char const* str : get_cmd_args())
    {
        if (std::strncmp(str, arg.data(), arg.size()) == 0)
            return true;
    }

    return false;
}

cc::string_view get_cmd_arg_value(cc::string_view arg)
{
    auto const curr_cmd_args = get_cmd_args();
    int const argc = int(curr_cmd_args.size());
    char const* const* argv = curr_cmd_args.data();

    // only iterate to the (n - 1)th arg
    for (auto i = 0; i < argc - 1; ++i)
    {
        if (!arg.equals(argv[i]))
            continue; // flag does not match

        char const* next_arg = argv[i + 1];

        if (!next_arg || next_arg[0] == '\0' || next_arg[0] == '-')
        {
            // the flag exists but its next argument is empty or starts with a dash as well
            return {};
        }

        return cc::string_view(next_arg);
    }

    return {};
}
}
