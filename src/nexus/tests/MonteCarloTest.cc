#include "MonteCarloTest.hh"

#include <clean-core/array.hh>
#include <clean-core/defer.hh>
#include <clean-core/span.hh>

#include <typed-geometry/feature/random.hh>

#include <nexus/test.hh>

#include <iostream> // DEBUG!

#include <map>

struct nx::MonteCarloTest::machine
{
    struct value_set
    {
        cc::vector<value> vars;
        cc::vector<function*> safe_generators;        // generators with no input arg of the same type
        cc::vector<function*> mutators_or_generators; // functions generating or mutating this type
        cc::vector<function*> invariants;             // invariants with at least one arg of this type

        bool can_safely_generate() const { return safe_generators.size() > 0; }
        bool has_values() const { return vars.size() > 0; }
    };

    std::map<std::type_index, value_set> values;
    cc::vector<function*> test_functions;
    cc::vector<function*> all_functions;
    MonteCarloTest const* test;

    explicit machine(MonteCarloTest const* test) : test(test) {}

    bool has_values_to_execute(function const& f) const
    {
        for (auto a : f.arg_types)
            if (!values.at(a).has_values())
                return false;
        return true;
    }

    static machine build(MonteCarloTest const& test, cc::span<function> funs)
    {
        auto a = cc::array<function*>(funs.size());
        for (size_t i = 0; i < a.size(); ++i)
            a[i] = &funs[i];
        return build(test, a);
    }
    static machine build(MonteCarloTest const& test, cc::span<function*> funs)
    {
        auto m = machine(&test);

        for (auto f : funs)
        {
            // touch args and return types
            for (auto a : f->arg_types)
                m.values[a];
            if (f->return_type != typeid(void))
                m.values[f->return_type];

            if (f->is_invariant) // invariants
            {
                for (auto a : f->arg_types)
                    m.values[a].invariants.push_back(f);
            }
            else // normal function
            {
                m.all_functions.push_back(f);

                // register generators
                if (f->return_type != typeid(void))
                {
                    auto& vs = m.values[f->return_type];

                    vs.mutators_or_generators.push_back(f);

                    auto is_safe = true;
                    for (auto a : f->arg_types)
                        if (a == f->return_type)
                            is_safe = false;

                    if (is_safe)
                        vs.safe_generators.push_back(f);
                }

                // register mutators
                for (auto i = 0; i < f->arity(); ++i)
                    if (f->arg_types_could_change[i])
                    {
                        auto& vs = m.values[f->arg_types[i]];
                        if (vs.mutators_or_generators.empty() || vs.mutators_or_generators.back() != f)
                            vs.mutators_or_generators.push_back(f);
                    }

                // init test function
                m.test_functions.push_back(f);
            }
        }

        return m;
    }

    bool is_properly_set_up() const
    {
        // sanity checks
        for (auto f : all_functions)
            for (auto a : f->arg_types)
                if (!values.at(a).can_safely_generate())
                {
                    std::cerr << "ERROR: no way to generate type " << a.name() << std::endl;
                    return false;
                }

        // sanity checks
        if (test_functions.empty())
        {
            std::cerr << "ERROR: no functions to test" << std::endl;
            return false;
        }

        return true;
    }

    int max_arity() const
    {
        auto a = 0;
        for (auto f : all_functions)
            a = tg::max(a, f->arity());
        return a;
    }

    void remove_fulfilled_test_functions()
    {
        for (auto i = int(test_functions.size()) - 1; i >= 0; --i)
        {
            if (test_functions[i]->executions >= test_functions[i]->min_executions)
            {
                std::swap(test_functions[i], test_functions.back());
                test_functions.pop_back();
            }
        }
    }

    void execute_invariants_for(value& v)
    {
        for (auto const& f : values.at(v.type).invariants)
        {
            CC_ASSERT(f->arity() == 1 && "currently only unary invariants supported");
            CC_ASSERT(f->arg_types[0] == v.type);
            value* vp = &v;
            auto iv = f->execute(cc::span<value*>(vp));
            if (iv.type == typeid(bool))
                CHECK(*static_cast<bool*>(iv.ptr));
        }
    };

    function* sample_suitable_test_function(tg::rng& rng, int max_tries = 500) const
    {
        // randomly choose new unsatisfied function
        auto f = random_choice(rng, test_functions);

        // find an executable function
        while (!has_values_to_execute(*f))
        {
            while (true) // no endless loop because at least one arg type must have no values
            {
                // get a random parameter with no values
                auto t = random_choice(rng, f->arg_types);
                auto const& vs = values.at(t);
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
                return nullptr;
            }
        }

        return f;
    }

    bool try_sample_args_with_precondition(tg::rng& rng, function* f, cc::span<value*> args, int max_tries = 10)
    {
        CC_ASSERT(has_values_to_execute(*f));
        CC_ASSERT(int(args.size()) == f->arity());
        auto const arity = f->arity();

        while (--max_tries >= 0)
        {
            // collect args
            for (auto i = 0; i < arity; ++i)
                args[i] = &random_choice(rng, values.at(f->arg_types[i]).vars);

            // check precondition
            if (!f->precondition || f->precondition(args))
                break;
        }

        return max_tries >= 0;
    }

    void execute(tg::rng& rng, function* f, cc::span<value*> args)
    {
        CC_ASSERT(int(args.size()) == f->arity());

        auto v = f->execute(args);
        f->executions++;

        // invariants for reference args
        for (size_t i = 0; i < f->arity(); ++i)
            if (f->arg_types_could_change[i])
                execute_invariants_for(*args[i]);

        if (!v.is_void())
        {
            // verify invariants
            execute_invariants_for(v);

            // add value (either new or replace)
            auto& vs = values.at(v.type);
            if (uniform(rng, 0.0f, 1.0f) <= 1 / (1.f + vs.vars.size()))
                vs.vars.emplace_back(cc::move(v));
            else
                random_choice(rng, vs.vars) = cc::move(v);
        }
    }

    void try_generate_values_for(tg::rng& rng, function* ref, cc::span<value*> arg_buffer)
    {
        CC_ASSERT(ref->arity() > 0 && "0-ary functions should never have failing preconditions");

        // 50/50 generating a suitable arg type or a random one
        function* f;
        if (rng() % 2 == 0)
        {
            auto a = random_choice(rng, ref->arg_types);
            f = random_choice(rng, values.at(a).mutators_or_generators);
        }
        else
            f = random_choice(rng, all_functions);

        // sample args and try to execute
        auto args = arg_buffer.subspan(0, f->arity());
        if (try_sample_args_with_precondition(rng, f, args, 1))
            execute(rng, f, args);
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
    // pre and post callbacks
    for (auto& f : mPreCallbacks)
        f();

    CC_DEFER
    {
        for (auto& f : mPostCallbacks)
            f();
    };

    // TODO: seed
    {
        tg::rng rng;

        // build machine
        auto m = machine::build(*this, mFunctions);
        REQUIRE(m.is_properly_set_up());

        // execute
        auto args_buffer = cc::array<value*>::filled(m.max_arity(), nullptr);
        auto unsuccessful_count = 0;
        while (!m.test_functions.empty())
        {
            if (unsuccessful_count > 1000)
            {
                std::cerr << "ERROR: unable to execute a test function (no precondition satisfied)" << std::endl;
                for (auto f : m.test_functions)
                    std::cerr << "  .. could not execute '" << f->name.c_str() << "'" << std::endl;
                CHECK(false);
                break;
            }

            // get function ot test
            auto f = m.sample_suitable_test_function(rng);
            if (f == nullptr)
                return;

            // collect some values
            auto args = cc::span<value*>(args_buffer.data(), f->arity());
            auto ok = m.try_sample_args_with_precondition(rng, f, args);
            if (!ok)
            {
                m.try_generate_values_for(rng, f, args_buffer);

                ++unsuccessful_count;
                continue;
            }

            // execute function
            m.execute(rng, f, args);
            unsuccessful_count = 0;

            // remove satisfied test functions
            m.remove_fulfilled_test_functions();
        }

        // ~Machine() at scope end
    }
}
