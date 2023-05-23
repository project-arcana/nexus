#pragma once

#include <cstddef>
#include <initializer_list>

#include <nexus/detail/make_string_repr.hh>

namespace nx
{
/// convenience function to CHECK ranges/iterables
/// usage:
///
///   cc::vector<int> v = ...;
///   CHECK(v == nx::range{1,2,3});
///   CHECK(v == nx::range<size_t>{1,2,3});
///
/// NOTE: do not save this class. it is only intended to be used within CHECK
///
template <class T>
struct range
{
    range(std::initializer_list<T> vals)
    {
        _data = vals.begin();
        _size = vals.size();
    }

    T const* begin() const { return _data; }
    T const* end() const { return _data + _size; }
    size_t size() const { return _size; }

    template <class Range>
    bool operator==(Range const& rhs) const
    {
        size_t i = 0;
        for (auto const& v : rhs)
        {
            using U = std::decay_t<decltype(v)>;
            static_assert(std::is_same_v<T, U>, "element types must match exactly to prevent errors");

            if (i >= _size)
                return false;

            if (_data[i] != v)
                return false;

            ++i;
        }

        return i == _size;
    }
    template <class Range>
    bool operator!=(Range const& rhs) const
    {
        return !operator==(rhs);
    }

private:
    T const* _data;
    size_t _size;
};

// C++20's rewrite rules makes this ambiguous
#if __cplusplus < 202002L
template <class Range, class T>
bool operator==(Range const& lhs, range<T> const& rhs)
{
    return rhs.operator==(lhs);
}
template <class Range, class T>
bool operator!=(Range const& lhs, range<T> const& rhs)
{
    return rhs.operator!=(lhs);
}
#endif
}
