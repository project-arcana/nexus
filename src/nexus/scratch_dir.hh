#pragma once

#include <clean-core/string.hh>

#include <nexus/detail/api.hh>

namespace nx
{
/// returns a path to a fresh, empty temporary directory, for file I/O in a test or app
/// ie. <TEMP>/arcana-nexus/tmpdata_mytest/
[[nodiscard]] NX_API cc::string open_scratch_directory();
}
