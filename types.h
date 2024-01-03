#pragma once

#include "enum-utils.h"

namespace Editor
{
    enum class Column : size_t
    {
        Beginning
    };

    enum class Length : size_t { };

    enum class CharOffset : size_t
    {
        Sentinel = sentinel_for<CharOffset>
    };

    constexpr CharOffset operator+(CharOffset off, Length len)
    {
        return CharOffset{ rep(off) + rep(len) };
    }

    constexpr Length distance(CharOffset first, CharOffset last)
    {
        return Length{ rep(last) - rep(first) };
    }

    constexpr Length operator+(Length lhs, Length rhs)
    {
        return Length{ rep(lhs) + rep(rhs) };
    }

    constexpr Length operator-(Length lhs, Length rhs)
    {
        return Length{ rep(lhs) - rep(rhs) };
    }
}