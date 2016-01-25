#pragma once

#include "up_detection_idiom.hpp"
#include "up_promote.hpp"
#include "up_string_view.hpp"

/**
 * The following namespace is used to find and invoke the correct overloaded,
 * non-member function 'to_string'. The candidate set will include functions
 * found via ADL as well as overloaded functions for primitive types.
 *
 * Using a dedicated namespace reduces the candidate set. It simplifies error
 * messages and avoids triggering an unintended conversion operation.
 */
namespace up_adl_to_string
{

    // required for primitive types
    using std::to_string;

    // for zero-terminated C-strings
    inline auto to_string(const char* text) -> std::string
    {
        return std::string(text);
    }

    inline auto to_string(const up::string_view& value) -> std::string
    {
        return {value.data(), value.size()};
    }

    inline auto to_string(const std::string& value) -> const std::string&
    {
        return value;
    }

    /**
     * The function returns a copy (constructed via move construction) in
     * order to avoid problems when storing the result of the function in a
     * local reference variable.
     */
    inline auto to_string(std::string&& value) -> std::string
    {
        return std::move(value);
    }


    // avoid choosing the overload with the const lvalue-reference
    auto to_string(const std::string&& value) -> std::string = delete;

    /**
     * Explicitly provide a template of integral types to avoid signed-ness
     * issues caused by integral promotion.
     */
    template <typename Type>
    auto to_string(Type value)
        -> std::enable_if_t<std::is_integral<Type>::value, std::string>
    {
        return std::to_string(up::promote(value));
    }

    /**
     * Explicitly provide a template of enum types to avoid signed-ness issues
     * caused by integral promotion.
     */
    template <typename Type>
    auto to_string(Type value)
        -> std::enable_if_t<std::is_enum<Type>::value, std::string>
    {
        return std::to_string(up::promote(static_cast<std::underlying_type_t<Type>>(value)));
    }


    /**
     * The substitution (intentionally) fails, if there is no matching
     * to_string function (SFINAE). This is useful to choose between member
     * and non-member functions.
     */
    template <typename... Args>
    auto invoke(Args&&... args)
        -> decltype(to_string(std::declval<Args>()...))
    {
        /* The following function invocation (intentionally) supports ADL, so
         * that to_string functions can be "overloaded" in the namespaces
         * corresponding to the types. */
        return to_string(std::forward<Args>(args)...);
    }

}

namespace up_to_string
{

    template <typename Type>
    using member_to_string_t = decltype(std::declval<Type>().to_string());

    template <typename Type>
    using has_member_to_string = up::is_detected<member_to_string_t, Type>;

    template <typename Type>
    using free_to_string_t = decltype(up_adl_to_string::invoke(std::declval<Type>()));

    template <typename Type>
    using has_free_to_string = up::is_detected<free_to_string_t, Type>;


    /**
     * Overloaded function template for invoking a member to_string function.
     * It will only take part in overload resolution if there is matching
     * member function.
     */
    template <typename Type>
    auto invoke_to_string(Type&& value)
        -> std::enable_if_t<
            has_member_to_string<Type>() || !has_free_to_string<Type>(),
            decltype(std::declval<Type>().to_string())>
    {
        return std::forward<Type>(value).to_string();
    }

    /**
     * Overloaded function template for invoking a non-member to_string
     * function. It will only take part in overload resolution if there is
     * matching non-member function.
     */
    template <typename Type>
    auto invoke_to_string(Type&& value)
        -> std::enable_if_t<
            !has_member_to_string<Type>() && has_free_to_string<Type>(),
            decltype(up_adl_to_string::invoke(std::declval<Type>()))>
    {
        return up_adl_to_string::invoke(std::forward<Type>(value));
    }

}

namespace up
{

    using up_to_string::invoke_to_string;

}
