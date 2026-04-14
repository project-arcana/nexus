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
    bool mNoEndless = false;
    bool mCatch2Mode = false;      // true when any Catch2 compat flag is present
    bool mHasListTests = false;    // --list-tests was passed
    bool mHasXmlReporter = false;  // --reporter was passed
    bool mVerbose = false;
    bool mRunDisabledTests = false; // run disabled tests when explicitly targeted
    cc::string mForceReproduction;
    cc::string mXmlOutputFile;
    int mTestArgC = 0;
    char const* const* mTestArgV = nullptr;
};
}
