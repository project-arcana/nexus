#include "trace_serialize.hh"

#include <clean-core/span.hh>
#include <clean-core/string.hh>
#include <clean-core/string_view.hh>
#include <clean-core/vector.hh>

static constexpr cc::string_view trace_charset = "-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static int find_char(char c)
{
    for (auto i = 0; i < int(trace_charset.size()); ++i)
        if (trace_charset[i] == c)
            return i;
    CC_ASSERT(false && "not found");
    return -1;
}

cc::string nx::detail::trace_encode(cc::span<const int> data)
{
    cc::string s;

    auto constexpr ccnt = int(trace_charset.size());

    for (auto i : data)
    {
        CC_ASSERT(i >= -1);

        i += 1;
        if (i < ccnt)
            s += trace_charset[i];
        else if (i < ccnt * ccnt)
        {
            s += '.';
            s += trace_charset[i % ccnt];
            s += trace_charset[i / ccnt];
        }
        else if (i < ccnt * ccnt * ccnt)
        {
            s += ':';
            s += trace_charset[i % ccnt];
            i /= ccnt;
            s += trace_charset[i % ccnt];
            s += trace_charset[i / ccnt];
        }
        else
            CC_ASSERT(false && "data too big");
    }

    return s;
}

cc::vector<int> nx::detail::trace_decode(cc::string_view encoded_string)
{
    auto pos = 0;

    auto constexpr ccnt = int(trace_charset.size());

    auto read_int = [&] {
        auto c = encoded_string[pos++];
        int i;
        if (c == ':')
        {
            auto c1 = encoded_string[pos++];
            auto c2 = encoded_string[pos++];
            auto c3 = encoded_string[pos++];
            i = (find_char(c3) * ccnt + find_char(c2)) * ccnt + find_char(c1);
        }
        else if (c == '.')
        {
            auto c1 = encoded_string[pos++];
            auto c2 = encoded_string[pos++];
            i = find_char(c2) * ccnt + find_char(c1);
        }
        else
            i = find_char(c);
        return i - 1;
    };

    cc::vector<int> v;

    while (pos < int(encoded_string.size()))
        v.push_back(read_int());

    return v;
}
