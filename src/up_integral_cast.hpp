#pragma once

/**
 * This file provides an easy way to convert from one integral type to another
 * with overflow checks. The primary design goal is to keep the overhead
 * small, in particular in cases where no check would be necessary. The casts
 * should be used whenever there are doubts, that the types might be
 * different, even if they are identical on a particular platform.
 */

#include "up_exception.hpp"

namespace up_integral_cast
{

    struct bad_integral_cast;


    template <typename Result, typename Type>
    auto integral_cast(Type value)
        -> typename std::enable_if<std::is_integral<Result>{} && std::is_integral<Type>{}, Result>::type
    {
        /* The following builtin is supported by both Clang and GCC, gives
         * reasonable good results, and is likely to be improved with future
         * compiler versions. */
        Result result;
        if (__builtin_add_overflow(value, Type(), &result)) {
            UP_RAISE(bad_integral_cast, "bad-integral-cast"_s, value);
        } else {
            return result;
        }
    }


    template <typename Type>
    class integral_caster_t final
    {
    private: // --- state ---
        Type _value;
    public: // --- life ---
        explicit integral_caster_t(Type value)
            : _value{value}
        { }
    public: // --- operations ---
        template <typename Result>
        operator Result() const &&
        {
            return integral_cast<Result, Type>(_value);
        }
    };

    template <typename Type>
    auto integral_caster(Type value)
    {
        return integral_caster_t<Type>{value};
    }

}

namespace up
{

    using up_integral_cast::integral_cast;
    using up_integral_cast::integral_caster;

}
