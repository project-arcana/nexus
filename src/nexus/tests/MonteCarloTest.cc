#include "MonteCarloTest.hh"

#include <iostream> // DEBUG!

#include <map>

struct nx::MonteCarloTest::machine
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

    machine m;

    // register generators
    for (auto& f : mFunctions)
        if (f.return_type != typeid(void))
            m.values[f.return_type].generators.push_back(&f);

    // checks
    for (auto const& [t, vs] : m.values)
    {
        if (vs.constants.size() + vs.generators.size() == 0)
        {
            std::cerr << "ERROR: no values and no way to generate type " << t.name() << std::endl;
            return;
        }
    }
}
