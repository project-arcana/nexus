#include "parse_arg.hh"

#include <iostream>
#include <sstream>

template <class T>
static bool sstream_convert(T& v, cc::string const& s, char const* tname)
{
    auto ss = std::istringstream(std::string(s.c_str()));
    ss >> v;
    if (ss.fail() || ss.bad() || !ss.eof())
    {
        std::cerr << "cannot convert argument `" << s.c_str() << "' to " << tname << std::endl;
        return false;
    }
    return true;
}

namespace nx::detail
{
bool parse_arg(char& v, cc::string const& s)
{
    if (s.size() != 1)
    {
        std::cerr << "cannot convert argument `" << s.c_str() << "' to char" << std::endl;
        return false;
    }

    v = s[0];
    return true;
}

bool parse_arg(short& v, cc::string const& s) { return sstream_convert(v, s, "int16"); }

bool parse_arg(unsigned short& v, cc::string const& s) { return sstream_convert(v, s, "uint16"); }

bool parse_arg(int& v, cc::string const& s) { return sstream_convert(v, s, "int32"); }

bool parse_arg(unsigned& v, cc::string const& s) { return sstream_convert(v, s, "uint32"); }

bool parse_arg(long& v, cc::string const& s) { return sstream_convert(v, s, "int64"); }

bool parse_arg(unsigned long& v, cc::string const& s) { return sstream_convert(v, s, "uint64"); }

bool parse_arg(long long& v, cc::string const& s) { return sstream_convert(v, s, "int64"); }

bool parse_arg(unsigned long long& v, cc::string const& s) { return sstream_convert(v, s, "uint64"); }

bool parse_arg(float& v, cc::string const& s) { return sstream_convert(v, s, "float"); }

bool parse_arg(double& v, cc::string const& s) { return sstream_convert(v, s, "double"); }

bool parse_arg(cc::string& v, cc::string const& s)
{
    v = s;
    return true;
}
}
