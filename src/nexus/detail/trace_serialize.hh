#pragma once

#include <clean-core/fwd.hh>
#include <clean-core/span.hh>

namespace nx::detail
{
cc::string trace_encode(cc::span<int const> data);
cc::vector<int> trace_decode(cc::string_view encoded_string);
}
