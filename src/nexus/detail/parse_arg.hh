#pragma once

#include <clean-core/string.hh>
#include <clean-core/string_view.hh>

namespace nx::detail
{
struct not_supported;

bool parse_arg(char& v, cc::string const& s);

bool parse_arg(short& v, cc::string const& s);
bool parse_arg(unsigned short& v, cc::string const& s);
bool parse_arg(int& v, cc::string const& s);
bool parse_arg(unsigned int& v, cc::string const& s);
bool parse_arg(long& v, cc::string const& s);
bool parse_arg(unsigned long& v, cc::string const& s);
bool parse_arg(long long& v, cc::string const& s);
bool parse_arg(unsigned long long& v, cc::string const& s);

bool parse_arg(float& v, cc::string const& s);
bool parse_arg(double& v, cc::string const& s);

bool parse_arg(cc::string& v, cc::string const& s);

// TODO: std types
// TODO: ranges

not_supported parse_arg(...);
}
