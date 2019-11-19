#include "MonteCarloTest.hh"

#include <clean-core/array.hh>
#include <clean-core/defer.hh>
#include <clean-core/pair.hh>
#include <clean-core/span.hh>

#include <typed-geometry/feature/random.hh>

#include <nexus/test.hh>

#include <iostream> // DEBUG!

#include <map>
#include <set>

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

    bool try_sample_args_with_precondition(tg::rng& rng, function* f, cc::span<value*> args, cc::span<int> arg_indices, int max_tries = 10)
    {
        CC_ASSERT(has_values_to_execute(*f));
        CC_ASSERT(int(args.size()) == f->arity());
        CC_ASSERT(int(arg_indices.size()) == f->arity());
        auto const arity = f->arity();

        while (--max_tries >= 0)
        {
            // collect args
            for (auto i = 0; i < arity; ++i)
            {
                auto& vars = values.at(f->arg_types[i]).vars;
                CC_ASSERT(!vars.empty());
                auto ai = uniform(rng, size_t(0), vars.size() - 1);
                args[i] = &vars[ai];
                arg_indices[i] = ai;
            }

            // check precondition
            if (!f->precondition || f->precondition(args))
                break;
        }

        return max_tries >= 0;
    }

    value execute(function* f, cc::span<value*> args)
    {
        CC_ASSERT(int(args.size()) == f->arity());

        auto v = f->execute(args);
        f->executions++;

        // invariants for reference args
        for (size_t i = 0; i < f->arity(); ++i)
            if (f->arg_types_could_change[i])
                execute_invariants_for(*args[i]);

        // verify invariants for return type
        if (!v.is_void())
            execute_invariants_for(v);

        return v;
    }

    void integrate_value(tg::rng& rng, value v)
    {
        if (v.is_void())
            return;

        // add value (either new or replace)
        auto& vs = values.at(v.type);
        if (uniform(rng, 0.0f, 1.0f) <= 1 / (1.f + vs.vars.size()))
            vs.vars.emplace_back(cc::move(v));
        else
            random_choice(rng, vs.vars) = cc::move(v);
    }

    function* try_generate_values_for(tg::rng& rng, function* ref, cc::span<value*> arg_buffer, cc::span<int> index_buffer)
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

        if (!has_values_to_execute(*f))
            return nullptr;

        // sample args and try to execute
        auto args = arg_buffer.subspan(0, f->arity());
        auto arg_indices = index_buffer.subspan(0, f->arity());
        if (try_sample_args_with_precondition(rng, f, args, arg_indices, 1))
            return f;
        else
            return nullptr;
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

    // post callbacks
    CC_DEFER
    {
        for (auto& f : mPostCallbacks)
            f();
    };

    // normal mode: no equivalence checking
    if (mEquivalences.empty())
    {
        // TODO: seed
        tg::rng rng;

        // build machine
        auto m = machine::build(*this, mFunctions);
        REQUIRE(m.is_properly_set_up());

        // execute
        auto args_buffer = cc::array<value*>::filled(m.max_arity(), nullptr);
        auto index_buffer = cc::array<int>::filled(m.max_arity(), -1);
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
            auto arg_indices = cc::span<int>(index_buffer.data(), f->arity());
            auto ok = m.try_sample_args_with_precondition(rng, f, args, arg_indices);
            if (!ok)
            {
                // execute other function
                if (auto ff = m.try_generate_values_for(rng, f, args_buffer, arg_indices))
                {
                    args = cc::span<value*>(args_buffer).subspan(0, ff->arity());
                    auto v = m.execute(ff, args);
                    m.integrate_value(rng, cc::move(v));
                }

                ++unsuccessful_count;
                continue;
            }

            // execute function
            auto v = m.execute(f, args);
            m.integrate_value(rng, cc::move(v));
            unsuccessful_count = 0;

            // remove satisfied test functions
            m.remove_fulfilled_test_functions();
        }

        // ~Machine() at scope end
    }
    else // equivalence checker
    {
        std::set<function*> eq_funs;
        std::map<std::type_index, std::map<std::string, function*>> eq_funs_by_type;

        // register functions related to the equivalences
        for (auto const& e : mEquivalences)
        {
            auto skip_a = bool(eq_funs_by_type.count(e.type_a));
            auto skip_b = bool(eq_funs_by_type.count(e.type_b));

            for (auto& f : mFunctions)
            {
                auto is_eq_a = false;
                auto is_eq_b = false;

                is_eq_a |= f.return_type == e.type_a;
                is_eq_b |= f.return_type == e.type_b;
                for (auto a : f.arg_types)
                {
                    is_eq_a |= a == e.type_a;
                    is_eq_b |= a == e.type_b;
                }

                CC_ASSERT(!(is_eq_a && is_eq_b) && "no function may 'bridge' between types checked for equivalence");

                if (is_eq_a || is_eq_b)
                    eq_funs.insert(&f);
                if (is_eq_a && !skip_a)
                {
                    CC_ASSERT(!eq_funs_by_type[e.type_a].count(f.name.c_str()) && "functions checked for equivalence need unique names");
                    eq_funs_by_type[e.type_a][f.name.c_str()] = &f;
                }
                if (is_eq_b && !skip_b)
                {
                    CC_ASSERT(!eq_funs_by_type[e.type_b].count(f.name.c_str()) && "functions checked for equivalence need unique names");
                    eq_funs_by_type[e.type_b][f.name.c_str()] = &f;
                }
            }
        }
        CC_ASSERT(!eq_funs.empty() && "no functions found to check for equivalence");

        // helper for collecting pairs of functions used for equivalence
        auto const collect_functions = [&](std::type_index ta, std::type_index tb) {
            cc::vector<cc::pair<function*, function*>> funs;

            // unrelated funs
            for (auto& f : mFunctions)
                if (!eq_funs.count(&f))
                    funs.emplace_back(&f, &f);

            // type related funs
            for (auto const& [name, fa] : eq_funs_by_type.at(ta))
            {
                if (!eq_funs_by_type.at(tb).count(name))
                {
                    std::cerr << "operation '" << name << "' not found for type " << tb.name() << std::endl;
                    std::cerr << "(note: an exact match is required, subtyping may interfere with this)" << std::endl;
                }
                CC_ASSERT(eq_funs_by_type.at(tb).count(name) && "all functions checked for equivalence need to be defined for both types");
                auto fb = eq_funs_by_type.at(tb).at(name);
                funs.emplace_back(fa, fb);

                // either both or neither can have a precondition
                REQUIRE(bool(fa->precondition) == bool(fb->precondition));
            }

            return funs;
        };

        // testing each equivalence
        for (auto const& e : mEquivalences)
        {
            // reset functions
            for (auto& f : mFunctions)
            {
                f.executions = 0;
                f.internal_idx = -1;
            }

            // prepare functions
            auto funs = collect_functions(e.type_a, e.type_b);
            cc::vector<function*> funs_a;
            cc::vector<function*> funs_b;
            for (auto i = 0; i < int(funs.size()); ++i)
            {
                auto f_a = funs[i].first;
                auto f_b = funs[i].second;

                CC_ASSERT(f_a->internal_idx == -1);
                CC_ASSERT(f_b->internal_idx == -1);

                f_a->internal_idx = i;
                f_b->internal_idx = i;

                funs_a.push_back(f_a);
                funs_b.push_back(f_b);

                if (f_a->return_type == e.type_a)
                    CC_ASSERT(f_b->return_type == e.type_b);
                else
                    CC_ASSERT(f_a->return_type == f_b->return_type);

                for (auto i = 0; i < f_a->arity(); ++i)
                {
                    CC_ASSERT(f_a->arg_types_could_change[i] == f_b->arg_types_could_change[i]);

                    if (f_a->arg_types[i] == e.type_a)
                        CC_ASSERT(f_b->arg_types[i] == e.type_b);
                    else
                        CC_ASSERT(f_a->arg_types[i] == f_b->arg_types[i]);
                }
            }

            // TODO: seed
            tg::rng rng;

            // prepare machines
            auto m_a = machine::build(*this, funs_a);
            auto m_b = machine::build(*this, funs_b);
            REQUIRE(m_a.is_properly_set_up());
            REQUIRE(m_b.is_properly_set_up());
            REQUIRE(m_a.max_arity() == m_b.max_arity());

            // helper functions
            auto const prepare_execution_b = [&m_b](function* f_b, cc::span<int> arg_indices, cc::span<value*> args_b) {
                CC_ASSERT(arg_indices.size() == args_b.size());
                CC_ASSERT(int(args_b.size()) == f_b->arity());

                for (auto i = 0; i < int(arg_indices.size()); ++i)
                {
                    auto tb = f_b->arg_types[i];
                    auto ai = arg_indices[i];
                    auto& vars = m_b.values.at(tb).vars;
                    CC_ASSERT(0 <= ai && ai < int(vars.size()));

                    args_b[i] = &vars[ai];
                }

                if (f_b->precondition)
                    CC_ASSERT(f_b->precondition(args_b) && "first precondition was true but second was not");
            };
            auto const bi_execution = [&m_a, &m_b, &e](tg::rng& rng, function* f_a, function* f_b, cc::span<value*> args_a, cc::span<value*> args_b) {
                CC_ASSERT(f_a->internal_idx == f_b->internal_idx);
                CC_ASSERT(f_a->arity() == f_b->arity());
                CC_ASSERT(f_a->arity() == int(args_a.size()));
                CC_ASSERT(f_b->arity() == int(args_b.size()));

                auto va = m_a.execute(f_a, args_a);
                auto vb = m_b.execute(f_b, args_b);

                // test equivalence
                if (va.type == e.type_a)
                {
                    CC_ASSERT(vb.type == e.type_b && "type mismatch");
                    e.test(va, vb);
                }
                for (auto i = 0; i < f_a->arity(); ++i)
                    if (f_a->arg_types_could_change[i] && f_a->arg_types[i] == e.type_a)
                    {
                        CC_ASSERT(f_b->arg_types[i] == e.type_b && "type mismatch");
                        e.test(*args_a[i], *args_b[i]);
                    }

                // reintegrate values
                auto rng_b = rng; // copy
                m_a.integrate_value(rng, cc::move(va));
                m_b.integrate_value(rng_b, cc::move(vb));
            };

            // execute
            auto args_buffer_a = cc::array<value*>::filled(m_a.max_arity(), nullptr);
            auto args_buffer_b = cc::array<value*>::filled(m_a.max_arity(), nullptr);
            auto index_buffer = cc::array<int>::filled(m_a.max_arity(), -1);
            auto unsuccessful_count = 0;
            while (!m_a.test_functions.empty())
            {
                if (unsuccessful_count > 1000)
                {
                    std::cerr << "ERROR: unable to execute a test function (no precondition satisfied)" << std::endl;
                    for (auto f : m_a.test_functions)
                        std::cerr << "  .. could not execute '" << f->name.c_str() << "'" << std::endl;
                    CHECK(false);
                    break;
                }

                // get function ot test
                auto f_a = m_a.sample_suitable_test_function(rng);
                if (f_a == nullptr)
                    return;
                auto f_b = funs_b[f_a->internal_idx];
                CC_ASSERT(f_a->arity() == f_b->arity());

                // collect some values
                auto args_a = cc::span<value*>(args_buffer_a.data(), f_a->arity());
                auto args_b = cc::span<value*>(args_buffer_b.data(), f_b->arity());
                auto arg_indices = cc::span<int>(index_buffer.data(), f_a->arity());
                auto ok = m_a.try_sample_args_with_precondition(rng, f_a, args_a, arg_indices);
                if (!ok)
                {
                    // execute other function
                    if (auto ff_a = m_a.try_generate_values_for(rng, f_a, args_buffer_a, index_buffer))
                    {
                        auto ff_b = funs_b[ff_a->internal_idx];
                        CC_ASSERT(ff_a->arity() == ff_b->arity());
                        arg_indices = cc::span<int>(index_buffer.data(), ff_a->arity());
                        args_a = cc::span<value*>(args_buffer_a.data(), ff_a->arity());
                        args_b = cc::span<value*>(args_buffer_b.data(), ff_b->arity());

                        prepare_execution_b(ff_b, arg_indices, args_b);
                        bi_execution(rng, ff_a, ff_b, args_a, args_b);
                    }

                    ++unsuccessful_count;
                    continue;
                }

                // build corresponding function
                prepare_execution_b(f_b, arg_indices, args_b);

                // execute function
                bi_execution(rng, f_a, f_b, args_a, args_b);
                unsuccessful_count = 0;

                // remove satisfied test functions
                m_a.remove_fulfilled_test_functions();
            }

            // ~Machine() at scope end
        }
    }
}

void nx::MonteCarloTest::printSetup()
{
    std::cout << "registered functions:" << std::endl;
    for (auto const& f : mFunctions)
    {
        std::cout << "  " << f.name.c_str() << " : (";
        for (auto i = 0; i < f.arity(); ++i)
        {
            if (i > 0)
                std::cout << ", ";
            std::cout << f.arg_types[i].name();
        }
        std::cout << ") -> " << f.return_type.name();
        if (f.is_invariant)
            std::cout << " [INVARIANT]";
        std::cout << std::endl;
    }
}
