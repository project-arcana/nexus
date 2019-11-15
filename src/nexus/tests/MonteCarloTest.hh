#pragma once

#include <typeindex>
#include <typeinfo>
#include <utility>

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
    // TODO: pointer-to-function-member

    template <class F>
    function& addInvariant(cc::string name, F&& f)
    {
        auto& fun = addOp(name, cc::forward<F>(f));
        CC_CONTRACT(fun.return_type == typeid(void));
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
    };

    struct function
    {
        // TODO: logging/printing inputs -> outputs

        int arity() const { return arg_types.size(); }

        function& execute_at_least(int cnt)
        {
            min_executions = cnt;
            return *this;
        }

        template <class F, class R, class... Args>
        function(cc::string name, F&& f, R (F::*op)(Args...)) : name(cc::move(name)), return_type(typeid(std::decay_t<R>))
        {
            (arg_types.emplace_back(typeid(std::decay_t<Args>)), ...);

            execute = [f = cc::forward<F>(f)](cc::span<value*> inputs) -> value {
                return executor<Args...>::template apply<std::decay_t<R>>(f, inputs, std::index_sequence_for<Args...>());
            };
        }

        template <class F, class R, class... Args>
        function(cc::string name, F&& f, R (F::*op)(Args...) const) : name(cc::move(name)), return_type(typeid(std::decay_t<R>))
        {
            (arg_types.emplace_back(typeid(std::decay_t<Args>)), ...);

            execute = [f = cc::forward<F>(f)](cc::span<value*> inputs) -> value {
                return executor<Args...>::template apply<std::decay_t<R>>(f, inputs, std::index_sequence_for<Args...>());
            };
        }

        template <class R, class... Args>
        function(cc::string name, R (*f)(Args...)) : name(cc::move(name)), return_type(typeid(std::decay_t<R>))
        {
            (arg_types.emplace_back(typeid(std::decay_t<Args>)), ...);

            execute = [f](cc::span<value*> inputs) -> value {
                return executor<Args...>::template apply<std::decay_t<R>>(f, inputs, std::index_sequence_for<Args...>());
            };
        }

    private:
        cc::string name;
        cc::unique_function<value(cc::span<value*>)> execute;
        cc::vector<std::type_index> arg_types;
        std::type_index return_type;
        bool is_invariant = false;
        int min_executions = 100;
        int executions = 0;

        friend class MonteCarloTest;
    };

    // members
private:
    cc::vector<function> mFunctions;
};
}
