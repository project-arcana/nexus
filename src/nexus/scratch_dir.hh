#pragma once

#include <clean-core/span.hh>
#include <clean-core/string.hh>

namespace nx
{
/// reads the OS temp directory, ie. /tmp/ or %APPDATA%/Local/Temp/
bool get_system_temp_directory(cc::span<char> out_path);

/// returns a path to a fresh, empty temporary directory, to test file I/O in a test or app
[[nodiscard]] cc::string open_scratch_directory();
}
