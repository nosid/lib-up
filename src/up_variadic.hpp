#pragma once

/*
 * This file is intended to provide helper functions and helper classes to
 * simplify the usage of variadic templates. Right now, it is only
 * experimental. In the future, things like 'pass' or 'apply' might be
 * added.
 */

#include "up_include.hpp"

namespace up_variadic
{

    constexpr bool variadic_all()
    {
        return true;
    }

    template <typename Head, typename... Args>
    constexpr bool variadic_all(Head&& head, Args&&... args)
    {
        return static_cast<bool>(std::forward<Head>(head))
            && variadic_all(std::forward<Args>(args)...);
    }

}

namespace up
{

    using up_variadic::variadic_all;

}
