#pragma once

#include "up_include.hpp"

namespace up_promote
{

    // integral promotion without changing signed-ness
    template <typename Type>
    auto promote(Type value)
        -> std::enable_if_t<std::is_integral<Type>() && std::is_signed<Type>(), std::make_signed_t<decltype(+value)>>
    {
        return {value};
    }

    // integral promotion without changing signed-ness
    template <typename Type>
    auto promote(Type value)
        -> std::enable_if_t<std::is_integral<Type>() && std::is_unsigned<Type>(), std::make_unsigned_t<decltype(+value)>>
    {
        return {value};
    }

}

namespace up
{

    using up_promote::promote;

}
