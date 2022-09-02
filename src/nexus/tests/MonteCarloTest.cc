#include "MonteCarloTest.hh"

#include <rich-log/log.hh>

#include <clean-core/array.hh>
#include <clean-core/assertf.hh>
#include <clean-core/defer.hh>
#include <clean-core/demangle.hh>
#include <clean-core/map.hh>
#include <clean-core/pair.hh>
#include <clean-core/set.hh>
#include <clean-core/span.hh>
#include <clean-core/vector.hh>

#include <typed-geometry/feature/random.hh>
#include <typed-geometry/functions/basic/minmax.hh>
#include <typed-geometry/tg-lean.hh>

#include <nexus/detail/assertions.hh>
#include <nexus/detail/exception.hh>
#include <nexus/detail/log.hh>
#include <nexus/detail/trace_serialize.hh>
#include <nexus/minimize_options.hh>
#include <nexus/test.hh>
#include <nexus/tests/Test.hh>

#include <cstdio>

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

    cc::map<std::type_index, value_set> values;
    cc::vector<function*> test_functions;
    cc::vector<function*> all_functions;
    MonteCarloTest const* test;

    explicit machine(MonteCarloTest const* test) : test(test) {}

    bool has_values_to_execute(function const& f) const
    {
        for (auto a : f.arg_types)
            if (!values.get(a).has_values())
                return false;
        return true;
    }

    static std::pair<machine, machine> build_equivalence_checker(MonteCarloTest& test, equivalence const& e, cc::vector<function*>& funs_a, cc::vector<function*>& funs_b)
    {
        cc::set<function*> eq_funs;
        cc::map<std::type_index, cc::map<cc::string, function*>> eq_funs_by_type;

        // register functions related to the equivalences
        for (auto const& e : test.mEquivalences)
        {
            auto skip_a = bool(eq_funs_by_type.contains_key(e.type_a));
            auto skip_b = bool(eq_funs_by_type.contains_key(e.type_b));

            for (auto& f : test.mFunctions)
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
                    eq_funs.add(&f);
                if (is_eq_a && !skip_a)
                {
                    CC_ASSERT(!eq_funs_by_type[e.type_a].contains_key(f.name.c_str()) && "functions checked for equivalence need unique names");
                    eq_funs_by_type[e.type_a][f.name.c_str()] = &f;
                }
                if (is_eq_b && !skip_b)
                {
                    CC_ASSERT(!eq_funs_by_type[e.type_b].contains_key(f.name.c_str()) && "functions checked for equivalence need unique names");
                    eq_funs_by_type[e.type_b][f.name.c_str()] = &f;
                }
            }
        }
        CC_ASSERT(!eq_funs.empty() && "no functions found to check for equivalence");

        // helper for collecting pairs of functions used for equivalence
        auto const collect_functions = [&](std::type_index ta, std::type_index tb)
        {
            cc::vector<cc::pair<function*, function*>> funs;

            // unrelated funs
            for (auto& f : test.mFunctions)
                if (!eq_funs.contains(&f) || f.is_invariant) // always add invariants
                    funs.emplace_back(&f, &f);

            // type related funs
            for (auto const& [name, fa] : eq_funs_by_type.get(ta))
            {
                if (!eq_funs_by_type.get(tb).contains_key(name))
                {
                    if (fa->is_optional)
                        continue; // not strictly required

                    LOG_ERROR("operation '{}' not found for type {}", name, tb.name());
                    LOG_ERROR("(note: an exact match is required, subtyping may interfere with this)");
                }
                CC_ASSERT(eq_funs_by_type.get(tb).contains_key(name) && "all functions checked for equivalence need to be defined for both types");
                auto fb = eq_funs_by_type.get(tb).get(name);
                funs.emplace_back(fa, fb);

                // either both or neither can have a precondition
                REQUIRE(bool(fa->precondition) == bool(fb->precondition));
            }

            return funs;
        };

        // collect functions
        auto funs = collect_functions(e.type_a, e.type_b);
        for (auto i = 0; i < int(funs.size()); ++i)
        {
            auto f_a = funs[i].first;
            auto f_b = funs[i].second;

            funs_a.push_back(f_a);
            funs_b.push_back(f_b);

            if (!f_a->is_invariant)
            {
                if (f_a->return_type == e.type_a)
                {
                    if (f_b->return_type != e.type_b)
                        LOG_ERROR("bisimulation return type mismatch for '{}': expected {}, got {}", f_a->name,
                             cc::demangle(e.type_b.name()), cc::demangle(f_b->return_type.name()));
                    CC_ASSERT(f_b->return_type == e.type_b);
                }
                else
                {
                    if (f_a->return_type != f_b->return_type)
                        LOG_ERROR("bisimulation return type mismatch for '{}': {} vs {}", f_a->name, cc::demangle(f_a->return_type.name()),
                             cc::demangle(f_b->return_type.name()));
                    CC_ASSERT(f_a->return_type == f_b->return_type);
                }

                for (auto i = 0; i < f_a->arity(); ++i)
                {
                    CC_ASSERT(f_a->arg_types_could_change[i] == f_b->arg_types_could_change[i]);

                    if (f_a->arg_types[i] == e.type_a)
                        CC_ASSERT(f_b->arg_types[i] == e.type_b);
                    else
                        CC_ASSERT(f_a->arg_types[i] == f_b->arg_types[i]);
                }
            }
        }

        return {machine::build(test, funs_a), machine::build(test, funs_b)};
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

        // reset functions
        for (auto f : funs)
        {
            f->executions = 0;
            f->internal_idx = -1;
        }

        for (int i = 0; i < int(funs.size()); ++i)
        {
            auto f = funs[i];

            // assign idx
            CC_ASSERT(f->internal_idx == -1);
            f->internal_idx = i;

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

        REQUIRE(m.is_properly_set_up());

        return m;
    }

    bool is_properly_set_up() const
    {
        // sanity checks
        for (auto f : all_functions)
            for (auto a : f->arg_types)
                if (!values.get(a).can_safely_generate())
                {
                    LOG_ERROR("no way to generate type {}", a.name());
                    return false;
                }

        // sanity checks
        if (test_functions.empty())
        {
            LOG_ERROR("no functions to test");
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
                auto const& vs = values.get(t);
                if (!vs.has_values())
                {
                    // try to execute a random safe generator
                    f = random_choice(rng, vs.safe_generators);
                    break;
                }
            }

            if (--max_tries < 0)
            {
                LOG_ERROR("unable to generate values of type {}", f->return_type.name());
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
                auto& vars = values.get(f->arg_types[i]).vars;
                CC_ASSERT(!vars.empty());
                auto ai = uniform(rng, size_t(0), vars.size() - 1);
                args[i] = &vars[ai];
                arg_indices[i] = int(ai);
            }

            // check precondition
            if (!f->precondition || f->precondition(args))
                break;
        }

        return max_tries >= 0;
    }

    value execute(function* f, cc::span<value*> args, bool exec_invariants = true)
    {
        CC_ASSERT(int(args.size()) == f->arity());

        auto v = f->execute(args);
        f->executions++;

        if (exec_invariants)
            execute_invariants_for(f, v, args);

        return v;
    }

    void execute_invariants_for(function* f, value& v, cc::span<value*> args)
    {
        // invariants for reference args
        for (int i = 0; i < f->arity(); ++i)
            if (f->arg_types_could_change[i])
                execute_invariants_for(*args[i]);

        // verify invariants for return type
        if (!v.is_void())
            execute_invariants_for(v);
    }

    void execute_invariants_for(value& v)
    {
        for (auto const& f : values.get(v.type).invariants)
        {
            CC_ASSERT(f->arity() == 1 && "currently only unary invariants supported");
            CC_ASSERT(f->arg_types[0] == v.type);
            value* vp = &v;
            auto iv = f->execute(cc::span<value*>(vp));
            if (iv.type == typeid(bool))
                CHECK(*static_cast<bool*>(iv.ptr));
        }
    };

    void integrate_value(value v, int idx)
    {
        if (v.is_void())
            return;

        CC_ASSERT(idx >= 0);
        auto& vs = values.get(v.type);

        // ensure proper size
        while (idx >= int(vs.vars.size()))
            vs.vars.push_back({});

        vs.vars[idx] = cc::move(v);
    }

    int generate_integrated_value_idx(tg::rng& rng, std::type_index type)
    {
        if (type == typeid(void))
            return -1;

        // add value (either new or replace)
        auto& vs = values.get(type);
        if (uniform(rng, 0.0f, 1.0f) <= 1 / (1.f + vs.vars.size()))
            return int(vs.vars.size());
        else
            return uniform(rng, 0, int(vs.vars.size()) - 1);
    }

    function* try_generate_values_for(tg::rng& rng, function* ref, cc::span<value*> arg_buffer, cc::span<int> index_buffer)
    {
        CC_ASSERT(ref->arity() > 0 && "0-ary functions should never have failing preconditions");

        // 50/50 generating a suitable arg type or a random one
        function* f;
        if (rng() % 2 == 0)
        {
            auto a = random_choice(rng, ref->arg_types);
            f = random_choice(rng, values.get(a).mutators_or_generators);
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
    machine_trace trace;

    auto test = nx::detail::get_current_test();

    CC_ASSERT(!test->shouldFail() && "should-fail tests not supported for MCT");
    CC_ASSERT(!test->isEndless() && "endless mode not YET supported for MCT");

    // reproduce trace
    if (test->shouldReproduce())
    {
        CC_ASSERT(!test->reproduction().trace.empty() && "MCT needs a string reproduce (trace)");
        LOG_ERROR("[nexus] replaying MCT trace '{}'", test->reproduction().trace);
        auto trace = nx::detail::trace_decode(test->reproduction().trace);
        reproduceTrace(trace);
        return;
    }

    if (test->isDebug())
    {
        tryExecuteMachineNormally(trace);
        return;
    }

    // prepare execution
    nx::detail::is_silenced() = true;
    nx::detail::always_terminate() = true;
    nx::detail::overwrite_assertion_handlers();

    // first: try normal execution
    try
    {
        tryExecuteMachineNormally(trace);
    }
    catch (nx::detail::assertion_failed_exception const&)
    {
        // on fail: try to minimize trace
        LOG_ERROR("[nexus] MONTE_CARLO_TEST failed. Trying to generate minimal reproduction.");
        minimizeTrace(trace);
        LOG_ERROR("[nexus] .. done. result:");

        // set reproduction BEFORE actually executing it
        test->setReproduce(reproduce(trace.serialize_to_string(*this)));

        nx::detail::is_silenced() = false;
        nx::detail::always_terminate() = false;
        fflush(stdout);
        fflush(stderr);
        replayTrace(trace, true);
        fflush(stdout);
        fflush(stderr);
    }

    nx::detail::always_terminate() = false;
    nx::detail::reset_assertion_handlers();
}

void nx::MonteCarloTest::tryExecuteMachineNormally(machine_trace& trace)
{
    auto verbose = detail::get_current_test()->isVerbose();

    // pre callbacks
    if (verbose)
        LOG(" .. executing pre-callbacks ({})", mPreCallbacks.size());
    for (auto& f : mPreCallbacks)
        f();

    // post callbacks
    CC_DEFER
    {
        if (verbose)
            LOG(" .. executing post-callbacks ({})", mPreCallbacks.size());
        for (auto& f : mPostCallbacks)
            f();
    };

    // helper
    auto const add_trace = [&trace, verbose](function* f, int vi, cc::span<int> arg_indices)
    {
        CC_ASSERT(f->arity() == int(arg_indices.size()));

        if (verbose)
            LOG(" .. execute [{}]", f->name);

        machine_trace::op op;
        op.function_idx = f->internal_idx;
        op.fun = f;
        op.args_start_idx = int(trace.arg_indices.size());
        op.return_value_idx = vi;
        trace.ops.push_back(op);
        for (auto ai : arg_indices)
            trace.arg_indices.push_back(ai);
    };

    // normal mode: no equivalence checking
    if (mEquivalences.empty())
    {
        tg::rng rng;
        rng.seed(get_seed());
        trace.start(nullptr);

        // build machine
        auto m = machine::build(*this, mFunctions);

        // execute
        auto args_buffer = cc::array<value*>::filled(m.max_arity(), nullptr);
        auto index_buffer = cc::array<int>::filled(m.max_arity(), -1);
        auto unsuccessful_count = 0;
        while (!m.test_functions.empty())
        {
            if (unsuccessful_count > 1000)
            {
                LOG_ERROR("unable to execute a test function (no precondition satisfied)");
                for (auto f : m.test_functions)
                    LOG_ERROR("  .. could not execute '{}'", f->name);
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
                if (auto ff = m.try_generate_values_for(rng, f, args_buffer, index_buffer))
                {
                    args = cc::span<value*>(args_buffer).subspan(0, ff->arity());

                    // add trace
                    auto vi = m.generate_integrated_value_idx(rng, ff->return_type);
                    add_trace(ff, vi, cc::span<int>(index_buffer.data(), ff->arity()));

                    // execute
                    auto v = m.execute(ff, args);
                    m.integrate_value(cc::move(v), vi);
                }

                ++unsuccessful_count;
                continue;
            }

            // add trace
            auto vi = m.generate_integrated_value_idx(rng, f->return_type);
            add_trace(f, vi, arg_indices);

            // execute function
            auto v = m.execute(f, args);
            m.integrate_value(cc::move(v), vi);
            unsuccessful_count = 0;

            // remove satisfied test functions
            m.remove_fulfilled_test_functions();
        }
    }
    else // equivalence checker
    {
        // testing each equivalence
        for (auto const& e : mEquivalences)
        {
            // seed rng
            tg::rng rng;
            rng.seed(get_seed());
            trace.start(&e);

            // prepare machines
            cc::vector<function*> funs_a;
            cc::vector<function*> funs_b;
            auto machines = machine::build_equivalence_checker(*this, e, funs_a, funs_b);
            auto& m_a = machines.first;
            auto& m_b = machines.second;
            REQUIRE(m_a.max_arity() == m_b.max_arity());

            // helper functions
            auto const prepare_execution_b = [&m_b](function* f_b, cc::span<int> arg_indices, cc::span<value*> args_b)
            {
                CC_ASSERT(arg_indices.size() == args_b.size());
                CC_ASSERT(int(args_b.size()) == f_b->arity());

                for (auto i = 0; i < int(arg_indices.size()); ++i)
                {
                    auto tb = f_b->arg_types[i];
                    auto ai = arg_indices[i];
                    auto& vars = m_b.values.get(tb).vars;
                    CC_ASSERT(0 <= ai && ai < int(vars.size()));

                    args_b[i] = &vars[ai];
                }

                if (f_b->precondition)
                    CC_ASSERT(f_b->precondition(args_b) && "first precondition was true but second was not");
            };
            auto const bi_execution = [this, &m_a, &m_b, &e, &add_trace](tg::rng& rng, function* f_a, function* f_b, cc::span<value*> args_a,
                                                                         cc::span<value*> args_b, cc::span<int> arg_indices)
            {
                CC_ASSERT(f_a->internal_idx == f_b->internal_idx);
                CC_ASSERT(f_a->arity() == f_b->arity());
                CC_ASSERT(f_a->arity() == int(args_a.size()));
                CC_ASSERT(f_b->arity() == int(args_b.size()));

                // add trace
                auto vi = m_a.generate_integrated_value_idx(rng, f_a->return_type);
                add_trace(f_a, vi, arg_indices);

                auto va = m_a.execute(f_a, args_a);
                auto vb = m_b.execute(f_b, args_b);

                // test equivalence
                if (va.type == e.type_a)
                {
                    CC_ASSERT(vb.type == e.type_b && "type mismatch");
                    e.test(va, vb);
                }
                else
                {
                    CC_ASSERT(va.type == vb.type && "type mismatch");
                    if (!va.is_void())
                    {
                        CC_ASSERT(mTypeMetadata.contains_key(va.type));
                        if (auto const& test_eq = mTypeMetadata.get(va.type).check_equality)
                            test_eq(va, vb);
                    }
                }

                for (auto i = 0; i < f_a->arity(); ++i)
                    if (f_a->arg_types_could_change[i])
                    {
                        if (f_a->arg_types[i] == e.type_a)
                        {
                            CC_ASSERT(f_b->arg_types[i] == e.type_b && "type mismatch");
                            e.test(*args_a[i], *args_b[i]);
                        }
                        else
                        {
                            CC_ASSERT(f_a->arg_types[i] == f_b->arg_types[i] && "type mismatch");
                            CC_ASSERT(mTypeMetadata.contains_key(f_a->arg_types[i]));
                            if (auto const& test_eq = mTypeMetadata.get(f_a->arg_types[i]).check_equality)
                                test_eq(*args_a[i], *args_b[i]);
                        }
                    }

                // reintegrate values
                m_a.integrate_value(cc::move(va), vi);
                m_b.integrate_value(cc::move(vb), vi);
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
                    LOG_ERROR(" unable to execute a test function (no precondition satisfied)");
                    for (auto f : m_a.test_functions)
                        LOG_ERROR("  .. could not execute '{}'", f->name);
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
                        bi_execution(rng, ff_a, ff_b, args_a, args_b, arg_indices);
                    }

                    ++unsuccessful_count;
                    continue;
                }

                // build corresponding function
                prepare_execution_b(f_b, arg_indices, args_b);

                // execute function
                bi_execution(rng, f_a, f_b, args_a, args_b, arg_indices);
                unsuccessful_count = 0;

                // remove satisfied test functions
                m_a.remove_fulfilled_test_functions();
            }
        }
    }
}

void nx::MonteCarloTest::minimizeTrace(machine_trace& trace)
{
    auto found_smaller = true;

    while (found_smaller) // TODO: time limit
    {
        LOG_ERROR("[nexus]   .. trace complexity {}", trace.complexity());
        auto opts = trace.build_minimizer();

        found_smaller = false;
        while (!found_smaller && opts.has_options_left())
        {
            // build new trace
            auto new_t = opts.get_next_option_for(trace);
            CC_ASSERT(new_t.complexity() < trace.complexity() && "operation did not reduce complexity");

            try
            {
                // try new trace
                replayTrace(new_t);
            }
            catch (nx::detail::assertion_failed_exception const&)
            {
                // found a smaller failing test
                trace = new_t;
                found_smaller = true;
            }
        }
    }
}

bool nx::MonteCarloTest::replayTrace(machine_trace const& trace, bool print_mode)
{
    if (print_mode)
        LOG_ERROR("[nexus] =============== TRACE BEGIN ===============");
    CC_DEFER
    {
        if (print_mode)
            LOG_ERROR("[nexus] =============== TRACE END ===============");
    };

    // pre callbacks
    if (print_mode && !mPreCallbacks.empty())
        LOG_ERROR("[nexus]   executing pre-callbacks");
    for (auto& f : mPreCallbacks)
        f();

    // post callbacks
    CC_DEFER
    {
        if (print_mode && !mPostCallbacks.empty())
            LOG_ERROR("[nexus]   executing post-callbacks");
        for (auto& f : mPostCallbacks)
            f();
    };

    // var names
    cc::map<cc::pair<std::type_index, int>, int> var_names;
    auto const get_var_name = [&var_names](std::type_index t, int vi)
    {
        if (var_names.contains_key({t, vi}))
            return var_names.get({t, vi});
        auto n = int(var_names.size());
        var_names[{t, vi}] = n;
        return n;
    };
    auto const value_to_string = [this](value const& v) -> cc::string
    {
        CC_ASSERT(!v.is_void());
        if (!mTypeMetadata.contains_key(v.type))
            return "???";
        auto const& f = mTypeMetadata.get(v.type).to_string;
        if (!f)
            return "???";
        return f(v.ptr);
    };

    // print symbolic execution
    if (print_mode)
    {
        LOG_ERROR("[nexus] symbolic:");
        for (auto const& op : trace.ops)
        {
            cc::string s;
            if (op.return_value_idx != -1)
            {
                s += "v";
                s += cc::to_string(get_var_name(op.fun->return_type, op.return_value_idx));
                s += " = ";
            }
            s += "[";
            s += op.fun->name.c_str();
            s += "]";
            if (op.fun->arity() > 0)
            {
                s += "(";
                for (auto ai = 0; ai < op.fun->arity(); ++ai)
                {
                    if (ai > 0)
                        s += ", ";
                    if (op.fun->arg_types_could_change[ai])
                        s += "&";
                    s += "v";
                    s += cc::to_string(get_var_name(op.fun->arg_types[ai], trace.arg_indices[op.args_start_idx + ai]));
                }
                s += ")";
            }
            if (op.return_value_idx != -1)
            {
                s += " : ";
                s += cc::demangle(op.fun->return_type.name());
            }
            LOG_ERROR("[nexus]   {}", s);
        }
        LOG_ERROR("[nexus] actual:");
    }
    auto const print_inputs = [&value_to_string](function* f, cc::span<value*> args, cc::span<cc::string> vals)
    {
        cc::string s;
        s += "exec [";
        s += f->name;
        s += "]";
        if (!args.empty())
        {
            s += "(";
            for (auto ai = 0; ai < f->arity(); ++ai)
            {
                if (ai > 0)
                    s += ", ";
                vals[ai] = value_to_string(*args[ai]);
                s += vals[ai].c_str();
            }
            s += ")";
        }
        LOG_ERROR("[nexus]   {}", s);
    };
    auto const print_outputs = [&value_to_string](cc::string_view prefix, function* f, value const& v, cc::span<value*> args, cc::span<const cc::string> vals)
    {
        cc::string s;
        s += "[";
        s += f->name;
        s += "]";
        if (!args.empty())
        {
            s += "(";
            for (auto ai = 0; ai < f->arity(); ++ai)
            {
                if (ai > 0)
                    s += ", ";
                auto vs = value_to_string(*args[ai]);
                if (f->arg_types_could_change[ai])
                {
                    s += vals[ai];
                    s += " -> ";
                    s += vs;
                }
                else
                {
                    // NOTE: values can alias, thus some args might have changed
                    // FIXME: CC_ASSERT(s == vals[ai] && "input should not have changed");
                    s += vs;
                }
            }
            s += ")";
        }
        if (f->return_type != typeid(void))
        {
            s += " -> ";
            s += value_to_string(v);
        }
        LOG_ERROR("[nexus]     {} {}", prefix, s);
    };

    // normal mode: no equivalence checking
    if (trace.equiv == nullptr)
    {
        tg::rng rng;
        rng.seed(get_seed());

        // build machine
        auto m = machine::build(*this, mFunctions);

        // execute
        auto args_buffer = cc::array<value*>::filled(m.max_arity(), nullptr);
        auto arg_string_buffer = cc::array<cc::string>::defaulted(m.max_arity());

        for (auto const& op : trace.ops)
        {
            auto f = &mFunctions[op.function_idx];
            CC_ASSERT(f == op.fun);

            // collect arguments
            auto args = cc::span<value*>(args_buffer.data(), f->arity());
            for (auto i = 0; i < f->arity(); ++i)
                args[i] = &m.values[f->arg_types[i]].vars[trace.arg_indices[op.args_start_idx + i]];

            // print trace
            if (print_mode)
                print_inputs(f, args, arg_string_buffer);

            // check precondition
            if (f->precondition && !f->precondition(args))
                return false; // precondition violated, i.e. invalid trace

            // execute function (no invariants)
            auto v = m.execute(f, args, false);

            // print outputs
            if (print_mode)
                print_outputs("  ", f, v, args, arg_string_buffer);

            // check invariants
            m.execute_invariants_for(f, v, args);

            // reintegrate values
            m.integrate_value(cc::move(v), op.return_value_idx);
        }
    }
    else // equivalence checker
    {
        auto const& e = *trace.equiv;

        // seed rng
        tg::rng rng;
        rng.seed(get_seed());

        // prepare machines
        cc::vector<function*> funs_a;
        cc::vector<function*> funs_b;
        auto machines = machine::build_equivalence_checker(*this, e, funs_a, funs_b);
        auto& m_a = machines.first;
        auto& m_b = machines.second;
        REQUIRE(m_a.max_arity() == m_b.max_arity());
        CC_ASSERT(funs_a.size() == funs_b.size());

        // execute
        auto args_buffer_a = cc::array<value*>::filled(m_a.max_arity(), nullptr);
        auto args_buffer_b = cc::array<value*>::filled(m_a.max_arity(), nullptr);
        auto arg_string_buffer_a = cc::array<cc::string>::defaulted(m_a.max_arity());
        auto arg_string_buffer_b = cc::array<cc::string>::defaulted(m_a.max_arity());
        for (auto const& op : trace.ops)
        {
            auto f_a = funs_a[op.function_idx];
            auto f_b = funs_b[op.function_idx];

            // collect arguments
            auto args_a = cc::span<value*>(args_buffer_a.data(), f_a->arity());
            auto args_b = cc::span<value*>(args_buffer_b.data(), f_b->arity());
            for (auto i = 0; i < f_a->arity(); ++i)
            {
                args_a[i] = &m_a.values[f_a->arg_types[i]].vars[trace.arg_indices[op.args_start_idx + i]];
                args_b[i] = &m_b.values[f_b->arg_types[i]].vars[trace.arg_indices[op.args_start_idx + i]];
            }

            // print trace
            if (print_mode)
            {
                print_inputs(f_a, args_a, arg_string_buffer_a);
                print_inputs(f_b, args_b, arg_string_buffer_b);
            }

            // check precondition
            if (f_a->precondition && !f_a->precondition(args_a))
                return false; // precondition violated, i.e. invalid trace
            if (f_b->precondition && !f_b->precondition(args_b))
                return false; // precondition violated, i.e. invalid trace

            // execute function
            auto va = m_a.execute(f_a, args_a, false);
            auto vb = m_b.execute(f_b, args_b, false);

            // print outputs
            if (print_mode)
            {
                print_outputs("A:", f_a, va, args_a, arg_string_buffer_a);
                print_outputs("B:", f_b, vb, args_b, arg_string_buffer_b);
            }

            // check invariants
            m_a.execute_invariants_for(f_a, va, args_a);
            m_b.execute_invariants_for(f_b, vb, args_b);

            // test equivalence
            if (va.type == e.type_a)
            {
                CC_ASSERT(vb.type == e.type_b && "type mismatch");
                e.test(va, vb);
            }
            else
            {
                CC_ASSERT(va.type == vb.type && "type mismatch");
                if (!va.is_void())
                {
                    CC_ASSERT(mTypeMetadata.contains_key(va.type));
                    if (auto const& test_eq = mTypeMetadata.get(va.type).check_equality)
                        test_eq(va, vb);
                }
            }

            for (auto i = 0; i < f_a->arity(); ++i)
                if (f_a->arg_types_could_change[i])
                {
                    if (f_a->arg_types[i] == e.type_a)
                    {
                        CC_ASSERT(f_b->arg_types[i] == e.type_b && "type mismatch");
                        e.test(*args_a[i], *args_b[i]);
                    }
                    else
                    {
                        CC_ASSERT(f_a->arg_types[i] == f_b->arg_types[i] && "type mismatch");
                        CC_ASSERT(mTypeMetadata.contains_key(f_a->arg_types[i]));
                        if (auto const& test_eq = mTypeMetadata.get(f_a->arg_types[i]).check_equality)
                            test_eq(*args_a[i], *args_b[i]);
                    }
                }

            // add values
            m_a.integrate_value(cc::move(va), op.return_value_idx);
            m_b.integrate_value(cc::move(vb), op.return_value_idx);
        }
    }

    return true;
}

void nx::MonteCarloTest::printSetup()
{
    LOG("registered functions:");
    for (auto const& f : mFunctions)
    {
        cc::string s;
        s += f.name;
        s += " : (";
        for (auto i = 0; i < f.arity(); ++i)
        {
            if (i > 0)
                s += ", ";
            s += f.arg_types[i].name();
        }
        s += ") -> ";
        s += f.return_type.name();
        if (f.is_invariant)
            s += " [INVARIANT]";
        LOG("  {}", s);
    }
}

void nx::MonteCarloTest::machine_trace::start(equivalence const* eq)
{
    equiv = eq;

    ops.clear();
    ops.reserve(500);

    arg_indices.clear();
    arg_indices.reserve(500);
}

nx::minimize_options<nx::MonteCarloTest::machine_trace> nx::MonteCarloTest::machine_trace::build_minimizer() const
{
    nx::minimize_options<nx::MonteCarloTest::machine_trace> min;

    struct var_info
    {
        bool has_reads = false;
        bool has_writes = false;
        int first_write = tg::max<int>();
        int last_read = -1;
    };

    struct varset_info
    {
        cc::vector<var_info> vars;

        void report_write(int step, int idx)
        {
            ensure_idx(idx);

            auto& v = vars[idx];
            v.has_writes = true;
            v.first_write = tg::min(step, v.first_write);
            // TODO
        }
        void report_read(int step, int idx)
        {
            ensure_idx(idx);

            auto& v = vars[idx];
            v.has_reads = true;
            v.last_read = tg::max(step, v.last_read);
            // TODO
        }
        void ensure_idx(int idx)
        {
            while (int(vars.size()) <= idx)
                vars.push_back({});
        }
    };

    cc::map<std::type_index, varset_info> varsets;

    // collect vars
    for (auto i = 0; i < int(ops.size()); ++i)
    {
        auto const& op = ops[i];

        if (op.return_value_idx != -1)
            varsets[op.fun->return_type].report_write(i, op.return_value_idx);

        for (auto ai = 0; ai < op.fun->arity(); ++ai)
        {
            auto& vs = varsets[op.fun->arg_types[ai]];
            auto idx = arg_indices[op.args_start_idx + ai];

            vs.report_read(i, idx);
            if (op.fun->arg_types_could_change[ai])
                vs.report_write(i, idx);
        }
    }

    auto can_disable_fun = cc::array<bool>::defaulted(ops.size());

    // try disabling functions
    auto disable_cnt = 0;
    for (int i = 0; i < int(ops.size()); ++i)
    {
        auto const& op = ops[i];

        auto can_disable = false;

        if (op.return_value_idx == -1) // return void
            can_disable = true;
        else
        {
            auto const& vi = varsets.get(op.fun->return_type).vars[op.return_value_idx];

            // not the first write
            if (i > vi.first_write)
                can_disable = true;

            // no read afterwards
            if (i > vi.last_read)
                can_disable = true;
        }

        can_disable_fun[i] = can_disable;
        if (can_disable)
            ++disable_cnt;
    }

    // exponential disable
    if (disable_cnt > 10)
    {
        auto seed = get_seed() + complexity();
        min.options.emplace_back(
            [can_disable_fun, seed](machine_trace const& old_t)
            {
                tg::rng rng;
                rng.seed(seed);
                auto t = old_t;
                t.ops.clear();
                for (auto i = 0; i < int(old_t.ops.size()); ++i)
                    if (!can_disable_fun[i] || tg::uniform<bool>(rng))
                        t.ops.push_back(old_t.ops[i]);
                t.ops.pop_back();
                return t;
            });
    }

    // disable single functions
    for (int i = 0; i < int(ops.size()); ++i)
        if (can_disable_fun[i])
            min.options.emplace_back(
                [i](machine_trace const& old_t)
                {
                    auto t = old_t;
                    for (auto j = i + 1; j < int(t.ops.size()); ++j)
                        t.ops[j - 1] = t.ops[j];
                    t.ops.pop_back();
                    return t;
                });

    // try to rename complete vars
    for (auto const& kvp : varsets)
    {
        auto type = kvp.key;
        CC_ASSERT(type != typeid(void));
        auto const& vs = kvp.value;
        for (int i_from = int(vs.vars.size()) - 1; i_from > 0; --i_from)
        {
            if (!vs.vars[i_from].has_reads && !vs.vars[i_from].has_writes)
                continue; // unused var slot

            for (int i_to = 0; i_to < i_from; ++i_to)
                min.options.emplace_back(
                    [type, i_from, i_to](machine_trace const& old_t)
                    {
                        auto t = old_t;
                        for (auto& op : t.ops)
                        {
                            // rename return val
                            if (op.fun->return_type == type && op.return_value_idx == i_from)
                                op.return_value_idx = i_to;

                            // rename args
                            for (auto ai = 0; ai < op.fun->arity(); ++ai)
                                if (op.fun->arg_types[ai] == type && t.arg_indices[op.args_start_idx + ai] == i_from)
                                    t.arg_indices[op.args_start_idx + ai] = i_to;
                        }
                        return t;
                    });
        }
    }

    // try to use different args
    for (int i = 0; i < int(ops.size()); ++i)
    {
        auto const& op = ops[i];
        for (auto ai = 0; ai < op.fun->arity(); ++ai)
        {
            auto vi = arg_indices[op.args_start_idx + ai];
            auto const& vs = varsets.get(op.fun->arg_types[ai]);
            for (auto j = 0; j < vi; ++j)
            {
                if (vs.vars[j].first_write >= i)
                    continue; // var was not written before

                min.options.emplace_back(
                    [i, ai, j](machine_trace const& old_t)
                    {
                        auto t = old_t;
                        auto const& op = t.ops[i];
                        t.arg_indices[op.args_start_idx + ai] = j;
                        return t;
                    });
            }
        }
    }

    return min;
}

int nx::MonteCarloTest::machine_trace::complexity() const
{
    auto c = 0;

    for (auto const& op : ops)
    {
        c++;

        if (op.return_value_idx >= 0)
            c += op.return_value_idx;

        for (auto i = 0; i < op.fun->arity(); ++i)
            c += arg_indices[op.args_start_idx + i];
    }

    return c;
}

void nx::MonteCarloTest::reproduceTrace(cc::span<int const> serialized_trace)
{
    machine_trace trace;
    { // deserialize trace
        auto pos = 0;
        auto const get_int = [&] { return serialized_trace[pos++]; };

        // equivalence and functions
        auto eq_idx = get_int();
        cc::vector<function*> funs;
        if (eq_idx == -1)
        {
            for (auto& f : mFunctions)
                funs.push_back(&f);
        }
        else
        {
            trace.equiv = &mEquivalences[eq_idx];

            cc::vector<function*> funs_b;
            machine::build_equivalence_checker(*this, *trace.equiv, funs, funs_b);
        }

        // rest of trace
        while (pos < int(serialized_trace.size()))
        {
            auto& op = trace.ops.emplace_back();
            op.function_idx = get_int();
            op.fun = funs[op.function_idx];
            op.return_value_idx = get_int();
            auto arity = get_int();
            CC_ASSERT(arity == op.fun->arity() && "invalid trace");
            op.args_start_idx = int(trace.arg_indices.size());
            for (auto ai = 0; ai < arity; ++ai)
                trace.arg_indices.push_back(get_int());
        }
    }

    replayTrace(trace, true);
}

cc::string nx::MonteCarloTest::machine_trace::serialize_to_string(MonteCarloTest const& test) const
{
    cc::vector<int> trace;

    // equiv
    if (!equiv)
        trace.push_back(-1);
    else
    {
        for (auto i = 0; i < int(test.mEquivalences.size()); ++i)
            if (&test.mEquivalences[i] == equiv)
            {
                trace.push_back(i);
                break;
            }
    }
    CC_ASSERT(trace.size() == 1);

    // serialize ops
    for (auto const& op : ops)
    {
        trace.push_back(op.function_idx);
        trace.push_back(op.return_value_idx);
        trace.push_back(op.fun->arity());
        for (auto ai = 0; ai < op.fun->arity(); ++ai)
            trace.push_back(arg_indices[op.args_start_idx + ai]);
    }

    return nx::detail::trace_encode(trace);
}
