#include "run.hh"

#include "Nexus.hh"

int nx::run(int argc, char** argv)
{
    Nexus n;
    n.applyCmdArgs(argc, argv);
    return n.run();
}
