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
    bool content_equals(Range const& rhs) const
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

private:
    T const* _data;
    size_t _size;
};

template <class Range, class T>
bool operator==(Range const& lhs, range<T> const& rhs)
{
    return rhs.content_equals(lhs);
}
template <class Range, class T>
bool operator!=(Range const& lhs, range<T> const& rhs)
{
    return !rhs.content_equals(lhs);
}

}
