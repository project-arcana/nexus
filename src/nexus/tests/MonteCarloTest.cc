#include "MonteCarloTest.hh"

#include <typed-geometry/feature/random.hh>

#include <nexus/test.hh>

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

void nx::MonteCarloTest::addPreSessionCallback(cc::unique_function<void()> f)
{
    CC_CONTRACT(f);
    mPreCallbacks.emplace_back(cc::move(f));
}

void nx::MonteCarloTest::addPostSessionCallback(cc::unique_function<void()> f)
{
    CC_CONTRACT(f);
    mPostCallbacks.emplace_back(cc::move(f));
}

void nx::MonteCarloTest::execute()
{
    // pre callbacks
    for (auto& f : mPreCallbacks)
        f();

    // TODO: seed
    {
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
            if (!f.is_invariant && f.return_type != typeid(void))
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
                auto iv = f->execute(cc::span<value*>(vp));
                if (iv.type == typeid(bool))
                    CHECK(*static_cast<bool*>(iv.ptr));
            }
        };

        // execute
        cc::vector<value*> args;
        auto unsuccessful_count = 0;
        while (!m.test_functions.empty())
        {
            if (unsuccessful_count > 1000)
            {
                std::cerr << "ERROR: unable to execute a test function (no precondition satisfied)" << std::endl;
                break;
            }

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

            // check precondition
            if (f->precondition && !f->precondition(args))
            {
                ++unsuccessful_count;
                continue;
            }

            // execute function
            auto v = f->execute(args);
            f->executions++;
            unsuccessful_count = 0;

            // invariants for reference args
            for (size_t i = 0; i < args.size(); ++i)
                if (f->arg_types_could_change[i])
                    execute_invariants_for(*args[i]);

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

        // ~Machine() at scope end
    }

    // post callbacks
    for (auto& f : mPostCallbacks)
        f();
}
