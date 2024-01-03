#pragma once

#include <concepts>
#include <limits>
#include <type_traits>

template <typename T>
concept Enum = std::is_enum_v<T>;

template <Enum E>
using PrimitiveType = std::underlying_type_t<E>;

template <Enum E>
constexpr auto sentinel_for = std::numeric_limits<PrimitiveType<E>>::max();

template <Enum E>
constexpr auto rep(E e) { return PrimitiveType<E>(e); }

template <Enum E>
constexpr E operator&(E a, E b) { return E(rep(a) & rep(b)); }

template <Enum E>
constexpr E operator|(E a, E b) { return E(rep(a) | rep(b)); }

template <Enum E>
constexpr E& operator&=(E& a, E b) { return a = a & b; }

template <Enum E>
constexpr E& operator|=(E& a, E b) { return a = a | b; }

template <Enum E>
constexpr auto retract(E e, PrimitiveType<E> x = 1) { return E(rep(e) - x); }

template <Enum E>
constexpr auto extend(E e, PrimitiveType<E> x = 1) { return E(rep(e) + x); }

template <Enum E>
constexpr bool implies(E a, E b) { return (a & b) == b; }

template <Enum E>
constexpr E unit = { };

template <typename E>
concept YesNoEnum = Enum<E>
                    && std::same_as<PrimitiveType<E>, bool>
                    && requires {
                        { E::Yes };
                        { E::No };
                    }
                    && rep(E::Yes) == true
                    && rep(E::No) == false;

template <YesNoEnum E>
constexpr bool is_yes(E e)
{
    return rep(e);
}

template <YesNoEnum E>
constexpr bool is_no(E e)
{
    return not rep(e);
}

template <YesNoEnum T>
constexpr T make_yes_no(bool value) noexcept
{
    return static_cast<T>(value);
}

template <typename T>
concept Countable = requires(T) {
    T::Count;
};

template <Countable T>
constexpr bool last_of(T t)
{
    return extend(t) == T::Count;
}

template <Countable T>
constexpr auto count_of = rep(T::Count);