#pragma once

/**
 * This file provides an easy way to convert from one integral type to another
 * with overflow checks. The primary design goal is to keep the overhead
 * small, in particular in cases where no check would be necessary. The casts
 * should be used whenever there are doubts, that the types might be
 * different, even if they are identical on a particular platform.
 */

#include <limits>

#include "up_exception.hpp"

namespace up_integral_cast
{

    struct bad_integral_cast;

    template <bool Value>
    using constant = std::integral_constant<bool, Value>;

    template <typename Target, typename Source>
    auto integral_cast_aux_same_sign(Source value, std::true_type)
    {
        return static_cast<Target>(value);
    }

    template <typename Target, typename Source>
    auto integral_cast_aux_same_sign(Source value, std::false_type)
    {
        if (value < Source{std::numeric_limits<Target>::min()}) {
            UP_RAISE(bad_integral_cast, "bad-integral-cast"_s, value);
        } else if (value > Source{std::numeric_limits<Target>::max()}) {
            UP_RAISE(bad_integral_cast, "bad-integral-cast"_s, value);
        } else {
            return static_cast<Target>(value);
        }
    }

    template <typename Target, typename Source>
    auto integral_cast_aux_from_signed_to_unsigned(Source value, std::true_type)
    {
        if (value < Source{0}) {
            UP_RAISE(bad_integral_cast, "bad-integral-cast"_s, value);
        } else {
            return static_cast<Target>(std::make_unsigned_t<Source>(value));
        }
    }

    template <typename Target, typename Source>
    auto integral_cast_aux_from_unsigned_to_signed(Source value, std::true_type)
    {
        if (value > Source{std::numeric_limits<Target>::max()}) {
            UP_RAISE(bad_integral_cast, "bad-integral-cast"_s, value);
        } else {
            return static_cast<Target>(value);
        }
    }

    template <typename Target, typename Source, bool sign>
    auto integral_cast_aux(Source value, constant<sign>, constant<sign>)
    {
        return integral_cast_aux_same_sign<Target>(value,
            constant<sizeof(Target) >= sizeof(Source)>{});
    }

    template <typename Target, typename Source>
    auto integral_cast_aux(Source value, std::false_type, std::true_type)
    {
        return integral_cast_aux_from_signed_to_unsigned<Target>(value,
            constant<sizeof(Target) >= sizeof(Source)>{});
    }

    template <typename Target, typename Source>
    auto integral_cast_aux(Source value, std::true_type, std::false_type)
    {
        return integral_cast_aux_from_unsigned_to_signed<Target>(value,
            constant<sizeof(Target) <= sizeof(Source)>{});
    }

    template <typename Target, typename Source>
    auto integral_cast(Source value)
    {
        static_assert(std::is_integral<Source>{}, "requires integral source type");
        static_assert(std::is_integral<Target>{}, "required integral target type");
        return integral_cast_aux<Target>(value, std::is_signed<Target>{}, std::is_signed<Source>{});
    }


    template <typename Source>
    class integral_caster_t final
    {
    private: // --- state ---
        Source _value;
    public: // --- life ---
        explicit integral_caster_t(Source value)
            : _value{value}
        { }
    public: // --- operations ---
        template <typename Target>
        operator Target() const &&
        {
            return integral_cast<Target>(_value);
        }
    };

    template <typename Source>
    auto integral_caster(Source value)
    {
        return integral_caster_t<Source>{value};
    }

}

namespace up
{

    using up_integral_cast::integral_cast;
    using up_integral_cast::integral_caster;

}
