#pragma once

#include <clean-core/string.hh>
#include <clean-core/vector.hh>

namespace nx
{
class Nexus
{
public:
    void applyCmdArgs(int argc, char** argv);
    int run();

private:
    cc::vector<cc::string> mSpecificTests;
    bool mPrintHelp = false;
    int mTestArgC = 0;
    char** mTestArgV = nullptr;
};
}
