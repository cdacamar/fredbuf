#pragma once

#include <concepts>
#include <utility>

template <std::invocable F>
class ScopeGuard
{
public:
    template <std::invocable T>
    explicit constexpr ScopeGuard(T&& scope_exit_func):
        scope_exit_func{ std::forward<T>(scope_exit_func) } { }

    ~ScopeGuard()
    {
        scope_exit_func();
    }
private:
    F scope_exit_func;
};

template <std::invocable F>
ScopeGuard(F&&) -> ScopeGuard<F>;