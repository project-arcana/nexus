#pragma once

#include <nexus/detail/api.hh>

namespace nx
{
/// easiest-to-use interface
/// equivalent to:
///
/// nx::Nexus nexus;
/// nexus.applyCmdArgs(argc, argv);
/// return nexus.run();
NX_API int run(int argc, char** argv);
}
