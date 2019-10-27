#pragma once

namespace nx
{
/// easiest-to-use interface
/// equivalent to:
///
/// nx::Nexus nexus;
/// nexus.applyCmdArgs(argc, argv);
/// return nexus.run();
int run(int argc, char** argv);
}
