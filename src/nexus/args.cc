#include "args.hh"

#include <clean-core/pair.hh>
#include <clean-core/set.hh>

#include <iostream>

#include <nexus/apps/App.hh>

namespace nx
{
args::args()
{
    if (auto const a = nx::detail::get_current_app())
        _app_name = a->name();
}

args::args(cc::string app_name, cc::string app_desc) : _app_name(cc::move(app_name)), _app_desc(cc::move(app_desc)) {}

args& args::disable_help()
{
    _disable_help = true;
    return *this;
}

args& args::group(cc::string name)
{
    _curr_group = cc::move(name);
    return *this;
}

args& args::version(cc::string v)
{
    _version = cc::move(v);
    return *this;
}

bool args::parse()
{
    if (auto a = nx::detail::get_current_app())
        return parse(a->argc(), a->argv());

    std::cerr << "no-arg version can only be used inside Nexus Apps" << std::endl;
    return false;
}

bool args::parse(int argc, char const* const* argv)
{
    _parsed_args.clear();
    _parsed_pos_args.clear();

    int pos_idx = 0;
    parsed_arg* valarg = nullptr;
    auto show_help = false;

    auto const add_pos_arg = [&](cc::string_view s) {
        // TODO: proper
        auto& pa = _parsed_pos_args.emplace_back();
        pa.values.push_back(s);
        ++pos_idx;
        return true;
    };

    auto const add_long_arg = [&](cc::string_view s) {
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
                        std::cerr << "argument `--" << cc::string(s).c_str() << "' has a value but should not have one." << std::endl;
                        return false;
                    }
                    return true;
                }

        std::cerr << "unknown argument `--" << cc::string(s).c_str() << "'" << std::endl;
        return false;
    };

    auto const add_short_args = [&](cc::string_view s) {
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
                std::cerr << "unknown argument `-" << s[i] << "'" << std::endl;
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
        std::cerr << "unexpected end of arguments. expected value after `" << argv[argc - 1] << "'" << std::endl;
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
            std::cerr << "argument `" << a.a->names.front().c_str() << "' was provided multiple times (this is only allowed for boolean/flag args)" << std::endl;
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

    return true;
}

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

    auto& a = _pos_args.emplace_back();
    a.metavar = n;
    a.description = cc::move(desc);
    return a;
}

void args::print_help()
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

    std::cout << std::endl;
}
}