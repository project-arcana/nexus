#pragma once

#include <typeindex>
#include <typeinfo>
#include <utility>

#include <clean-core/capped_vector.hh>
#include <clean-core/span.hh>
#include <clean-core/string.hh>
#include <clean-core/unique_function.hh>
#include <clean-core/vector.hh>

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
    template <class R, class... Args>
    function& addOp(cc::string name, R (*f)(Args...))
    {
        mFunctions.emplace_back(cc::move(name), f);
        return mFunctions.back();
    }
    template <class F>
    function& addOp(cc::string name, F&& f)
    {
        mFunctions.emplace_back(cc::move(name), cc::forward<F>(f), &F::operator());
        return mFunctions.back();
    }
    template <class Obj, class R, class... Args>
    function& addOp(cc::string name, R (Obj::*f)(Args...))
    {
        return addOp(name, [f](Obj& obj, Args... args) -> R { return (obj.*f)(cc::forward<Args>(args)...); });
    }
    template <class Obj, class R, class... Args>
    function& addOp(cc::string name, R (Obj::*f)(Args...) const)
    {
        return addOp(name, [f](Obj const& obj, Args... args) -> R { return (obj.*f)(cc::forward<Args>(args)...); });
    }
    template <class Obj, class R>
    function& addOp(cc::string name, R Obj::*f)
    {
        return addOp(name, [f](Obj& obj) -> R { return obj.*f; });
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

    // TODO: testSubstitutability(...)

    void addPreSessionCallback(cc::unique_function<void()> f) { mPreCallbacks.emplace_back(cc::move(f)); }
    void addPostSessionCallback(cc::unique_function<void()> f) { mPostCallbacks.emplace_back(cc::move(f)); }

    // execution
public:
    void execute();

    // impls
private:
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
        static value apply(F&& f, cc::span<value*> inputs, std::index_sequence<I...>)
        {
            // TODO: proper rvalue ref support (maybe via forward?)
            if constexpr (std::is_same_v<R, void>)
            {
                f((*static_cast<std::decay_t<Args>*>(inputs[I]->ptr))...);
                return {};
            }
            else
                return new R(f((*static_cast<std::decay_t<Args>*>(inputs[I]->ptr))...));
        }

        template <class F, size_t... I>
        static bool predicate(F&& f, cc::span<value*> inputs, std::index_sequence<I...>)
        {
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
        function(cc::string name, F&& f, R (F::*)(Args...)) : name(cc::move(name)), return_type(typeid(std::decay_t<R>))
        {
            init<decltype(f), R, Args...>(cc::forward<F>(f));
        }

        template <class F, class R, class... Args>
        function(cc::string name, F&& f, R (F::*)(Args...) const) : name(cc::move(name)), return_type(typeid(std::decay_t<R>))
        {
            init<decltype(f), R, Args...>(cc::forward<F>(f));
        }

        template <class R, class... Args>
        function(cc::string name, R (*f)(Args...)) : name(cc::move(name)), return_type(typeid(std::decay_t<R>))
        {
            init<decltype(f), R, Args...>(f);
        }

        template <class F>
        function& when(F&& f)
        {
            impl_set_precondition(cc::forward<F>(f), &F::operator());
            return *this;
        }
        template <class R, class... Args>
        function& when(R (*f)(Args...))
        {
            set_precondition<decltype(f), R, Args...>(f);
            return *this;
        }
        template <class Obj, class R, class... Args>
        function& when(R (Obj::*f)(Args...))
        {
            return when([f](Obj& obj, Args... args) -> R { return (obj.*f)(cc::forward<Args>(args)...); });
        }
        template <class Obj, class R, class... Args>
        function& when(R (Obj::*f)(Args...) const)
        {
            return when([f](Obj const& obj, Args... args) -> R { return (obj.*f)(cc::forward<Args>(args)...); });
        }
        template <class Obj, class R>
        function& when(R Obj::*f)
        {
            return when([f](Obj& obj) -> R { return obj.*f; });
        }

    private:
        template <class F, class R, class... Args>
        void init(F f)
        {
            (arg_types.emplace_back(typeid(std::decay_t<Args>)), ...);
            (arg_types_could_change.push_back(std::is_reference_v<Args> && !std::is_const_v<std::remove_reference_t<Args>>), ...);

            execute = [f = cc::forward<F>(f)](cc::span<value*> inputs) -> value {
                return executor<Args...>::template apply<std::decay_t<R>>(f, inputs, std::index_sequence_for<Args...>());
            };
        }

        template <class F, class R, class... Args>
        void impl_set_precondition(F&& f, R (F::*)(Args...))
        {
            set_precondition<F, R, Args...>(cc::forward<F>(f));
        }
        template <class F, class R, class... Args>
        void impl_set_precondition(F&& f, R (F::*)(Args...) const)
        {
            set_precondition<F, R, Args...>(cc::forward<F>(f));
        }
        template <class F, class R, class... Args>
        void set_precondition(F&& f)
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
                return executor<Args...>::predicate(f, inputs, std::index_sequence_for<Args...>());
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

        friend class MonteCarloTest;
    };

    // members
private:
    cc::vector<function> mFunctions;
    cc::vector<cc::unique_function<void()>> mPreCallbacks;
    cc::vector<cc::unique_function<void()>> mPostCallbacks;
};
}
