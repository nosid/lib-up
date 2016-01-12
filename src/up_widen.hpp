#pragma once

#include "up_include.hpp"

namespace up_widen
{

    template <bool Condition, template <typename...> typename True, template <typename...> typename False, typename... Types>
    class select;

    template <template <typename...> typename True, template <typename...> typename False, typename... Types>
    class select<true, True, False, Types...> : public True<Types...> { };

    template <template <typename...> typename True, template <typename...> typename False, typename... Types>
    class select<false, True, False, Types...> : public False<Types...> { };


    template <bool True>
    using bool_constant = std::integral_constant<bool, True>;

    template <typename...>
    using true_t = bool_constant<true>;

    template <typename...>
    using false_t = bool_constant<false>;


    template <typename Type>
    constexpr auto widest(Type value) -> std::enable_if_t<
        select<std::is_integral<Type>::value, std::is_signed, false_t, Type>::value, std::intmax_t>
    {
        return {value};
    }

    template <typename Type>
    constexpr auto widest(Type value) -> std::enable_if_t<
        select<std::is_integral<Type>::value, std::is_unsigned, false_t, Type>::value, std::uintmax_t>
    {
        return {value};
    }


    template <typename To, typename From>
    using is_widen_base = bool_constant<
        std::is_signed<To>::value == std::is_signed<From>::value
        && widest(std::numeric_limits<To>::min()) <= widest(std::numeric_limits<From>::min())
        && widest(std::numeric_limits<To>::max()) >= widest(std::numeric_limits<From>::max())>;

    template <typename To, typename From>
    using is_widen = select<
        std::is_integral<To>::value && std::is_integral<From>::value,
        is_widen_base, false_t, To, From>;


    template <typename From>
    class widener final
    {
    private: // --- state ---
        From _value;
    public: // --- life ---
        constexpr explicit widener(From value) noexcept
            : _value(value)
        {
            static_assert(noexcept(From(value)), "requires noexcept");
        }
    public: // --- operations ---
        template <typename To, typename = std::enable_if_t<is_widen<To, From>::value>>
        constexpr operator To() const noexcept
        {
            static_assert(noexcept(To{_value}), "requires noexcept");
            return {_value};
        }
    };


    template <typename To>
    class widen_t final
    {
    public: // --- scope ---
        template <typename From>
        constexpr static auto widen(From value) noexcept -> To
        {
            static_assert(noexcept(To{value}), "requires noexcept");
            return {value};
        }
    };

    template <>
    class widen_t<void> final
    {
    public: // --- scope ---
        template <typename From>
        constexpr static auto widen(From value) noexcept -> widener<From>
        {
            static_assert(noexcept(widener<From>(value)), "requires noexcept");
            return widener<From>(value);
        }
    };


    template <typename To = void, typename From, typename = std::enable_if_t<
        std::is_void<To>::value ? std::is_integral<From>::value : is_widen<To, From>::value>>
    auto widen(From value)
    {
        return widen_t<To>::widen(value);
    }

}

namespace up
{

    using up_widen::widen;

}
