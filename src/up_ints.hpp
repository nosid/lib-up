#pragma once

/**
 * This file provides an easy way to convert from one integral type to another
 * with overflow checks. The primary design goal is to keep the overhead
 * small, in particular in cases where no check would be necessary. The casts
 * should be used whenever there are doubts, that the types might be
 * different, even if they are identical on a particular platform.
 */

#include "up_include.hpp"

namespace up_ints
{

    class ints final
    {
    public: // --- scope ---

        struct bad_cast;

        template <typename Result, typename Type>
        static auto cast(Type value)
            -> typename std::enable_if<std::is_integral<Result>{} && std::is_integral<Type>{}, Result>::type
        {
            /* The following builtin is supported by both Clang and GCC, gives
             * reasonable good results, and is likely to be improved with
             * future compiler versions. */
            Result result;
            if (__builtin_add_overflow(value, Type(), &result)) {
                throw std::range_error("up-ints-bad-cast");
                // TODO: UP_RAISE(bad_cast, "up-ints-bad-cast"_sl, value);
            } else {
                return result;
            }
        }

        template <typename Type>
        class caster_t;

        template <typename Type>
        static auto caster(Type value)
        {
            return caster_t<Type>{value};
        }

        template <typename Type>
        class domain;

    };

    template <typename Type>
    class ints::caster_t final
    {
    private: // --- state ---
        Type _value;
    public: // --- life ---
        explicit caster_t(Type value)
            : _value{value}
        { }
    public: // --- operations ---
        template <typename Result>
        operator Result() const &&
        {
            return cast<Result, Type>(_value);
        }
    };


    template <typename Type>
    class ints::domain final
    {
    public: // --- scope ---
        template <typename Map>
        class ops;
        class map;
        using is_valid = ops<typename map::is_valid>;
        using unsafe = ops<typename map::unsafe>;
        template <typename Tag, typename Base>
        using or_exception = ops<typename map::template or_exception<Tag, Base>>;
        template <typename Tag>
        using or_length_error = or_exception<Tag, std::length_error>;
        template <typename Tag>
        using or_overflow_error = or_exception<Tag, std::overflow_error>;
        template <typename Tag>
        using or_range_error = or_exception<Tag, std::range_error>;
    };


    template <typename Type>
    template <typename Map>
    class ints::domain<Type>::ops final
    {
    private: // --- scope ---
        static auto _sum() -> std::pair<Type, bool>
        {
            return {{}, true};
        }
        template <typename Head, typename... Tail>
        static auto _sum(Head head, Tail... tail) -> std::pair<Type, bool>
        {
            auto partial = _sum(tail...);
            Type result;
            if (__builtin_add_overflow(head, partial.first, &result)) {
                return {result, false};
            } else {
                return {result, partial.second};
            }
        }

    public:
        template <typename Lhs, typename Rhs>
        static auto add(Lhs lhs, Rhs rhs)
        {
            Type result;
            bool valid = !__builtin_add_overflow(lhs, rhs, &result);
            return Map::map(result, valid);
        }
        template <typename Lhs, typename Rhs>
        static auto sub(Lhs lhs, Rhs rhs)
        {
            Type result;
            bool valid = !__builtin_sub_overflow(lhs, rhs, &result);
            return Map::map(result, valid);
        }
        template <typename Lhs, typename Rhs>
        static auto mul(Lhs lhs, Rhs rhs) -> Type
        {
            Type result;
            bool valid = !__builtin_mul_overflow(lhs, rhs, &result);
            return Map::map(result, valid);
        }
        template <typename... Args>
        static auto sum(Args... args) -> Type
        {
            auto result = _sum(args...);
            return Map::map(result.first, result.second);
        }
    };


    template <typename Type>
    class ints::domain<Type>::map final
    {
    public: // --- scope ---
        class is_valid;
        class unsafe;
        template <typename Tag, typename Base = std::range_error>
        class or_exception;
    };


    template <typename Tag, typename Base>
    class tagged final : public Base
    {
    public: // --- life ---
        using Base::Base;
    };


    template <typename Type>
    class ints::domain<Type>::map::is_valid final
    {
    public: // --- scope ---
        static auto map(Type value __attribute__((unused)), bool valid) -> bool
        {
            return valid;
        }
    };


    template <typename Type>
    class ints::domain<Type>::map::unsafe final
    {
    public: // --- scope ---
        static auto map(Type value, bool valid __attribute__((unused))) -> Type
        {
            return value;
        }
    };


    template <typename Type>
    template <typename Tag, typename Base>
    class ints::domain<Type>::map::or_exception final
    {
    public: // --- scope ---
        static auto map(Type value, bool valid) -> Type
        {
            if (valid) {
                return value;
            } else {
                throw tagged<Tag, Base>("up-ints-domain-map-exception");
            }
        }
    };

}

namespace up
{

    using up_ints::ints;

}
