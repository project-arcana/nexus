#include "Test.hh"

#include <clean-core/format.hh>

#include "MonteCarloTest.hh"

cc::string nx::Test::makeCurrentReproductionCommand() const
{
    if (mMCT)
    {
        auto tr = mMCT->mCurrentTrace;
        CC_ASSERT(tr != nullptr && "no current trace (must be called during execution)");
        return cc::format("reproduce(\"%s\")", tr->serialize_to_string(*mMCT));
    }

    CC_UNREACHABLE("non-MCT not implemented currently");
}

void nx::Test::setFirstFailInfo(const char* check, const char* file, int line, char const* function)
{
    if (mFirstFailMessage.empty())
    {
        mFirstFailMessage = check;
        mFirstFailFile = file;
        mFirstFailFunction = function;
        mFirstFailLine = line;
    }
}

cc::string nx::Test::makeFirstFailInfo() const
{
    if (mFirstFailMessage.empty())
        return "unknown check failed. probably an assertion.";

    return cc::format("'%s' failed in %s:%s", mFirstFailMessage, mFirstFailFile, mFirstFailLine);
}

cc::string nx::Test::makeFirstFailMessage() const
{
    if (mFirstFailMessage.empty())
        return "unknown check failed. probably an assertion.";
    return mFirstFailMessage;
}
