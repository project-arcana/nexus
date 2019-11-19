#pragma once

#include <typeindex>
#include <typeinfo>
#include <utility>

#include <map> // TODO: replace with cc!

#include <clean-core/capped_vector.hh>
#include <clean-core/span.hh>
#include <clean-core/string.hh>
#include <clean-core/unique_function.hh>
#include <clean-core/vector.hh>

#include <nexus/check.hh>
#include <nexus/detail/signature.hh>

namespace nx
{
class MonteCarloTest
{
    // fwd
private:
    struct machine;
    struct value;
    struct constant;
    struct function;

    // setup
public:
    template <class F>
    function& addOp(cc::string name, F&& f)
    {
        mFunctions.emplace_back(cc::move(name), detail::make_function(cc::forward<F>(f)), detail::make_signature(cc::forward<F>(f)));
        return mFunctions.back();
    }

    template <class F>
    function& addInvariant(cc::string name, F&& f)
    {
        auto& fun = addOp(name, cc::forward<F>(f));
        CC_ASSERT((fun.return_type == typeid(void) || fun.return_type == typeid(bool)) && "invariants must return void or bool");
        CC_CONTRACT(fun.arity() > 0);
        fun.is_invariant = true;
        return fun;
    }

    template <class T>
    void addValue(cc::string name, T&& v)
    {
        auto& f = addOp(cc::move(name), [v = cc::forward<T>(v)] { return v; });
        f.execute_at_least(0); // optional
    }

    void addPreSessionCallback(cc::unique_function<void()> f);
    void addPostSessionCallback(cc::unique_function<void()> f);

    template <class F>
    void testEquivalence(F&& test)
    {
        implTestEquivalence(detail::make_function(cc::forward<F>(test)), detail::make_signature(cc::forward<F>(test)));
    }
    template <class A, class B>
    void testEquivalence()
    {
        testEquivalence([](A const& a, B const& b) { CHECK(a == b); });
    }

    template <class T, class F>
    void setPrinter(F&& f)
    {
        mTypeMetadata[std::type_index(typeid(T))].to_string = [f = cc::move(f)](void* p) { return f(*static_cast<T const*>(p)); };
    }

    // execution
public:
    void execute();

    // impls
private:
    template <class F, class R, class A, class B>
    void implTestEquivalence(F&& test, detail::signature<R(A, B)>)
    {
        static_assert(std::is_invocable_v<F, A const&, B const&>, "function must be callable with (A const&, B const&)");
        auto& eq = mEquivalences.emplace_back(typeid(A), typeid(B));
        if constexpr (std::is_same_v<R, void>)
            eq.test = [test = cc::move(test)](value const& va, value const& vb) {
                auto const& a = *static_cast<std::decay_t<A> const*>(va.ptr);
                auto const& b = *static_cast<std::decay_t<B> const*>(vb.ptr);
                test(a, b);
            };
        else if constexpr (std::is_same_v<R, bool>)
            eq.test = [test = cc::move(test)](value const& va, value const& vb) {
                auto const& a = *static_cast<std::decay_t<A> const*>(va.ptr);
                auto const& b = *static_cast<std::decay_t<B> const*>(vb.ptr);
                CHECK(test(a, b));
            };
        else
            static_assert(cc::always_false<A, B>, "equivalence test must either return void or bool");
    }

    struct value
    {
        using deleter_t = void (*)(void*);

        void* ptr = nullptr;
        deleter_t deleter = nullptr;
        std::type_index type;

        value() : type(typeid(void)) {}

        bool is_void() const { return type == typeid(void); }

        template <class T>
        value(T* v) : ptr(v), deleter([](void* p) { delete static_cast<T*>(p); }), type(typeid(T))
        {
        }
        value(value&& rhs) noexcept : type(rhs.type)
        {
            ptr = rhs.ptr;
            deleter = rhs.deleter;
            rhs.ptr = nullptr;
            rhs.deleter = nullptr;
        }
        value& operator=(value&& rhs) noexcept
        {
            if (ptr)
                deleter(ptr);

            ptr = rhs.ptr;
            deleter = rhs.deleter;
            type = rhs.type;
            rhs.ptr = nullptr;
            rhs.deleter = nullptr;

            return *this;
        }
        ~value()
        {
            if (ptr)
                deleter(ptr);
        }
    };

    struct constant
    {
        cc::string name;
        cc::unique_function<value()> make_value;
    };

    template <class... Args>
    struct executor
    {
        template <class R, class F, size_t... I>
        static R apply(F&& f, cc::span<value*> inputs, std::index_sequence<I...>)
        {
            // TODO: proper rvalue ref support (maybe via forward?)
            return f((*static_cast<std::decay_t<Args>*>(inputs[I]->ptr))...);
        }
    };

    struct function
    {
        // TODO: logging/printing inputs -> outputs

        int arity() const { return int(arg_types.size()); }

        function& execute_at_least(int cnt)
        {
            min_executions = cnt;
            return *this;
        }

        template <class F, class R, class... Args>
        function(cc::string name, F&& f, detail::signature<R(Args...)>) : name(cc::move(name)), return_type(typeid(std::decay_t<R>))
        {
            (arg_types.emplace_back(typeid(std::decay_t<Args>)), ...);
            (arg_types_could_change.push_back(std::is_reference_v<Args> && !std::is_const_v<std::remove_reference_t<Args>>), ...);

            execute = [f = cc::forward<F>(f)](cc::span<value*> inputs) -> value {
                if constexpr (std::is_same_v<R, void>)
                {
                    executor<Args...>::template apply<std::decay_t<R>>(f, inputs, std::index_sequence_for<Args...>());
                    return {};
                }
                else
                    return new std::decay_t<R>(executor<Args...>::template apply<std::decay_t<R>>(f, inputs, std::index_sequence_for<Args...>()));
            };
        }

        template <class F>
        function& when(F&& f)
        {
            set_precondition(detail::make_function(cc::forward<F>(f)), detail::make_signature(cc::forward<F>(f)));
            return *this;
        }

        template <class F>
        function& when_not(F&& f)
        {
            return when(detail::compose(detail::make_function(cc::forward<F>(f)), [](auto&& v) { return !v; }, detail::make_signature(cc::forward<F>(f))));
        }

        template <class F, class V>
        function& when_equal(F&& f, V&& value)
        {
            return when(detail::compose(detail::make_function(cc::forward<F>(f)), [value = cc::forward<V>(value)](auto&& v) { return v == value; },
                                        detail::make_signature(cc::forward<F>(f))));
        }

        template <class F, class V>
        function& when_not_equal(F&& f, V&& value)
        {
            return when(detail::compose(detail::make_function(cc::forward<F>(f)), [value = cc::forward<V>(value)](auto&& v) { return v != value; },
                                        detail::make_signature(cc::forward<F>(f))));
        }

        template <class F, class V>
        function& when_greater_than(F&& f, V&& value)
        {
            return when(detail::compose(detail::make_function(cc::forward<F>(f)), [value = cc::forward<V>(value)](auto&& v) { return v > value; },
                                        detail::make_signature(cc::forward<F>(f))));
        }

        template <class F, class V>
        function& when_greater_or_equal(F&& f, V&& value)
        {
            return when(detail::compose(detail::make_function(cc::forward<F>(f)), [value = cc::forward<V>(value)](auto&& v) { return v >= value; },
                                        detail::make_signature(cc::forward<F>(f))));
        }

        template <class F, class V>
        function& when_less_than(F&& f, V&& value)
        {
            return when(detail::compose(detail::make_function(cc::forward<F>(f)), [value = cc::forward<V>(value)](auto&& v) { return v < value; },
                                        detail::make_signature(cc::forward<F>(f))));
        }

        template <class F, class V>
        function& when_less_or_equal(F&& f, V&& value)
        {
            return when(detail::compose(detail::make_function(cc::forward<F>(f)), [value = cc::forward<V>(value)](auto&& v) { return v <= value; },
                                        detail::make_signature(cc::forward<F>(f))));
        }

    private:
        template <class F, class R, class... Args>
        void set_precondition(F&& f, detail::signature<R(Args...)>)
        {
            CC_ASSERT(!precondition && "already has a precondition");
            static_assert(std::is_same_v<R, bool>, "precondition must return bool");
            // check correct argument types
            CC_ASSERT(sizeof...(Args) == arg_types.size() && "precondition arguments must match op arguments");
            cc::capped_vector<std::type_index, sizeof...(Args)> p_types;
            (p_types.emplace_back(typeid(std::decay_t<Args>)), ...);
            for (size_t i = 0; i < sizeof...(Args); ++i)
                CC_ASSERT(p_types[i] == arg_types[i] && "precondition arguments types must match op arguments");

            precondition = [f = cc::forward<F>(f)](cc::span<value*> inputs) -> bool {
                return executor<Args...>::template apply<bool>(f, inputs, std::index_sequence_for<Args...>());
            };
        }

    private:
        cc::string name;
        cc::unique_function<value(cc::span<value*>)> execute;
        cc::unique_function<bool(cc::span<value*>)> precondition;
        cc::vector<std::type_index> arg_types;
        cc::vector<bool> arg_types_could_change;
        std::type_index return_type;
        bool is_invariant = false;
        int min_executions = 100;
        int executions = 0;
        int internal_idx = -1;

        friend class MonteCarloTest;
    };

    struct equivalence
    {
        std::type_index type_a;
        std::type_index type_b;
        cc::unique_function<void(value const&, value const&)> test;

        equivalence(std::type_index a, std::type_index b) : type_a(a), type_b(b) {}
    };

    struct type_metadata
    {
        cc::unique_function<cc::string(void*)> to_string;
    };

    // members
private:
    std::map<std::type_index, type_metadata> mTypeMetadata;
    cc::vector<function> mFunctions;
    cc::vector<cc::unique_function<void()>> mPreCallbacks;
    cc::vector<cc::unique_function<void()>> mPostCallbacks;
    cc::vector<equivalence> mEquivalences;
};
}
