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


    template <typename Result, typename Type>
    using is_widen_base = bool_constant<
        std::is_signed<Result>::value == std::is_signed<Type>::value
        && widest(std::numeric_limits<Result>::min()) <= widest(std::numeric_limits<Type>::min())
        && widest(std::numeric_limits<Result>::max()) >= widest(std::numeric_limits<Type>::max())>;

    template <typename Result, typename Type>
    using is_widen = select<
        std::is_integral<Result>::value && std::is_integral<Type>::value,
        is_widen_base, false_t, Result, Type>;


    template <typename Type>
    class widener final
    {
    private: // --- state ---
        Type _value;
    public: // --- life ---
        constexpr explicit widener(Type value) noexcept
            : _value(value)
        {
            static_assert(noexcept(Type(value)), "requires noexcept");
        }
    public: // --- operations ---
        template <typename Result, typename = std::enable_if_t<is_widen<Result, Type>::value>>
        constexpr operator Result() const noexcept
        {
            static_assert(noexcept(Result{_value}), "requires noexcept");
            return {_value};
        }
    };


    template <typename Result>
    class widen_t final
    {
    public: // --- scope ---
        template <typename Type>
        constexpr static auto widen(Type value) noexcept -> Result
        {
            static_assert(noexcept(Result{value}), "requires noexcept");
            return {value};
        }
    };

    template <>
    class widen_t<void> final
    {
    public: // --- scope ---
        template <typename Type>
        constexpr static auto widen(Type value) noexcept -> widener<Type>
        {
            static_assert(noexcept(widener<Type>(value)), "requires noexcept");
            return widener<Type>(value);
        }
    };


    template <typename Result = void, typename Type, typename = std::enable_if_t<
        std::is_void<Result>::value ? std::is_integral<Type>::value : is_widen<Result, Type>::value>>
    auto widen(Type value)
    {
        return widen_t<Result>::widen(value);
    }

}

namespace up
{

    using up_widen::widen;

}
