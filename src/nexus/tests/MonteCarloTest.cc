#include "MonteCarloTest.hh"

#include <iostream> // DEBUG!

#include <map>

class nx::MonteCarloTest::Machine
{
    struct value_set
    {
        cc::vector<constant*> constants;
        cc::vector<value> vars;
        cc::vector<function*> generators; // matching return value type
    };

    std::map<std::type_index, value_set> values;
};

void nx::MonteCarloTest::execute()
{
    // TODO
    std::cout << "got " << mFunctions.size() << " funs and " << mConstants.size() << " constants" << std::endl;
}
