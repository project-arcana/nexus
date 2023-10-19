#pragma once

#include <clean-core/string.hh>
#include <clean-core/vector.hh>

#include <nexus/detail/api.hh>

namespace nx
{
class NX_API Nexus
{
public:
    void applyCmdArgs(int argc, char** argv);
    int run();

private:
    cc::vector<cc::string> mSpecificTests;
    cc::vector<cc::string> mEnabledGroups;
    bool mPrintHelp = false;
    bool mForceEndless = false;
    cc::string mForceReproduction;
    cc::string mXmlOutputFile;
    int mTestArgC = 0;
    char const* const* mTestArgV = nullptr;
};
}
