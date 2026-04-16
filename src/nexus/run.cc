#include "run.hh"

#include "Nexus.hh"

#include <rich-log/logger.hh>

int nx::run(int argc, char** argv)
{
    rlog::auto_detect_colors();

    Nexus n;
    n.applyCmdArgs(argc, argv);
    return n.run();
}
