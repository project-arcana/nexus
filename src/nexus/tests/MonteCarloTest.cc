#include "MonteCarloTest.hh"

#include <typed-geometry/feature/random.hh>

#include <iostream> // DEBUG!

#include <map>

struct nx::MonteCarloTest::machine
{
    struct value_set
    {
        cc::vector<value> vars;
        cc::vector<function*> generators;      // matching return value type
        cc::vector<function*> safe_generators; // generators with no input arg of the same type
        cc::vector<function*> invariants;      // invariants with at least one arg of this type

        bool can_safely_generate() const { return generators.size() > 0; }
        bool has_values() const { return vars.size() > 0; }
    };

    std::map<std::type_index, value_set> values;
    cc::vector<function*> test_functions;

    bool can_execute(function const& f) const
    {
        for (auto a : f.arg_types)
            if (!values.at(a).has_values())
                return false;
        return true;
    }
};

void nx::MonteCarloTest::execute()
{
    // TODO: seed
    tg::rng rng;
    machine m;

    // register functions
    for (auto& f : mFunctions)
    {
        // register invariants
        if (f.is_invariant)
            for (auto a : f.arg_types)
                m.values[a].invariants.push_back(&f);

        // register generators
        if (f.return_type != typeid(void))
        {
            auto& vs = m.values[f.return_type];

            vs.generators.push_back(&f);

            auto is_safe = true;
            for (auto a : f.arg_types)
                if (a == f.return_type)
                    is_safe = false;

            if (is_safe)
                vs.safe_generators.push_back(&f);
        }

        // init test function
        if (!f.is_invariant)
            m.test_functions.push_back(&f);
    }

    // sanity checks
    for (auto const& f : mFunctions)
        for (auto a : f.arg_types)
            if (!m.values[a].can_safely_generate())
            {
                std::cerr << "ERROR: no way to generate type " << a.name() << std::endl;
                return;
            }

    // sanity checks
    if (m.test_functions.empty())
    {
        std::cerr << "ERROR: no functions to test" << std::endl;
        return;
    }

    // test invariants
    auto const execute_invariants_for = [&](value& v) {
        for (auto const& f : m.values.at(v.type).invariants)
        {
            CC_ASSERT(f->arity() == 1 && "currently only unary invariants supported");
            CC_ASSERT(f->arg_types[0] == v.type);
            value* vp = &v;
            f->execute(cc::span<value*>(vp));
        }
    };

    // execute
    cc::vector<value*> args;
    while (!m.test_functions.empty())
    {
        // randomly choose new unsatisfied function
        auto f = random_choice(rng, m.test_functions);

        // find an executable function
        auto max_tries = 500;
        while (!m.can_execute(*f))
        {
            while (true) // no endless loop because at least one arg type must have no values
            {
                // get a random parameter with no values
                auto t = random_choice(rng, f->arg_types);
                auto const& vs = m.values.at(t);
                if (!vs.has_values())
                {
                    // try to execute a random safe generator
                    f = random_choice(rng, vs.safe_generators);
                    break;
                }
            }

            if (--max_tries < 0)
            {
                std::cerr << "ERROR: unable to generate values of type " << f->return_type.name() << std::endl;
                return;
            }
        }

        // generate some values
        CC_ASSERT(m.can_execute(*f));
        args.clear();
        for (auto a : f->arg_types)
            args.push_back(&random_choice(rng, m.values.at(a).vars));
        auto v = f->execute(args);
        f->executions++;

        if (!v.is_void())
        {
            // verify invariants
            execute_invariants_for(v);

            // add value (either new or replace)
            auto& vs = m.values.at(v.type);
            if (uniform(rng, 0.0f, 1.0f) <= 1 / (1.f + vs.vars.size()))
                vs.vars.emplace_back(cc::move(v));
            else
                random_choice(rng, vs.vars) = cc::move(v);
        }

        // remove satisfied test functions
        for (auto i = int(m.test_functions.size()) - 1; i >= 0; --i)
        {
            if (m.test_functions[i]->executions >= m.test_functions[i]->min_executions)
            {
                std::swap(m.test_functions[i], m.test_functions.back());
                m.test_functions.pop_back();
            }
        }
    }
}
