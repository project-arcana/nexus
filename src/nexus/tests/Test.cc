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
