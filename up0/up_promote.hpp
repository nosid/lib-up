#pragma once

namespace up_promote
{

    // integral promotion without changing signed-ness
    template <typename Type, typename = std::enable_if_t<std::is_integral<Type>::value && std::is_signed<Type>::value>>
    auto promote(Type value) -> std::make_signed_t<decltype(+value)>
    {
        return {value};
    }

    // integral promotion without changing signed-ness
    template <typename Type, typename = std::enable_if_t<std::is_integral<Type>::value && std::is_unsigned<Type>::value>>
    auto promote(Type value) -> std::make_unsigned_t<decltype(+value)>
    {
        return {value};
    }

}

namespace up
{

    using up_promote::promote;

}
