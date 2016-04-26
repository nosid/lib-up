#pragma once

namespace up_widen
{

    template <
        typename...,
        typename Type,
        typename = std::enable_if_t<std::is_integral<Type>::value && std::is_signed<Type>::value>>
    constexpr auto widest(Type value) -> std::intmax_t
    {
        return {value};
    }

    template <
        typename...,
        typename Type,
        typename = std::enable_if_t<std::is_integral<Type>::value && std::is_unsigned<Type>::value>>
    constexpr auto widest(Type value) -> std::uintmax_t
    {
        return {value};
    }


    template <
        typename Result,
        typename Type,
        bool = std::is_integral<Result>::value && std::is_integral<Type>::value>
    class is_wider_integral final
        : public std::bool_constant<
        std::is_signed<Result>::value == std::is_signed<Type>::value
        && widest(std::numeric_limits<Result>::min()) <= widest(std::numeric_limits<Type>::min())
        && widest(std::numeric_limits<Result>::max()) >= widest(std::numeric_limits<Type>::max())>
    { };

    template <typename Result, typename Type>
    class is_wider_integral<Result, Type, false> final
        : public std::bool_constant<false>
    { };


    template <typename Type>
    class widener final
    {
    private: // --- state ---
        Type _value;
    public: // --- life ---
        constexpr explicit widener(Type value) noexcept
            : _value(value)
        {
            static_assert(noexcept(Type(value)));
        }
    public: // --- operations ---
        template <
            typename Result,
            typename = std::enable_if_t<is_wider_integral<Result, Type>::value>>
        constexpr operator Result() const noexcept
        {
            static_assert(noexcept(Result{_value}));
            return {_value};
        }
    };


    template <typename Result>
    class widen_t final
    {
    public: // --- scope ---
        template <typename..., typename Type>
        constexpr static auto widen(Type value) noexcept -> Result
        {
            static_assert(noexcept(Result{value}));
            return {value};
        }
    };

    template <>
    class widen_t<void> final
    {
    public: // --- scope ---
        template <typename..., typename Type>
        constexpr static auto widen(Type value) noexcept -> widener<Type>
        {
            static_assert(noexcept(widener<Type>(value)));
            return widener<Type>(value);
        }
    };


    template <
        typename Result = void,
        typename...,
        typename Type,
        typename = std::enable_if_t<
            std::conditional_t<
                std::is_void<Result>::value,
                std::is_integral<Type>,
                is_wider_integral<Result, Type>>::value>>
    auto widen(Type value)
    {
        return widen_t<Result>::widen(value);
    }

}

namespace up
{

    using up_widen::widen;

}
