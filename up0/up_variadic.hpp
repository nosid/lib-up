#pragma once

/*
 * This file is intended to provide helper functions and helper classes to
 * simplify the usage of variadic templates. Right now, it is only
 * experimental. In the future, things like 'pass' or 'apply' (C++14) might be
 * added.
 */

namespace up_variadic
{

    template <typename... Args>
    constexpr bool variadic_all(Args&&... args)
    {
        return (true && ... && static_cast<bool>(std::forward<Args>(args)));
    }

}

namespace up
{

    using up_variadic::variadic_all;

}
