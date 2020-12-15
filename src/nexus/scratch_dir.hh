#pragma once

#include <clean-core/span.hh>
#include <clean-core/string.hh>

#include <nexus/detail/api.hh>

namespace nx
{
/// reads the OS temp directory, ie. /tmp/ or %APPDATA%/Local/Temp/
NX_API bool get_system_temp_directory(cc::span<char> out_path);

/// returns a path to a fresh, empty temporary directory, to test file I/O in a test or app
[[nodiscard]] NX_API cc::string open_scratch_directory();
}
