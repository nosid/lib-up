#pragma once

#include "up_include.hpp"

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

    /**
     * Internal helper class to check if a to_string member or non-member
     * function exists, that can be invoked with the given arguments (using
     * SFINAE). This class is required to choose between member and non-member
     * function.
     */
    class invoke_to_string_aux final
    {
    private:
        template <typename Head, typename... Tail>
        static auto test_member(Head&&, Tail&&...) noexcept
            -> decltype(std::declval<Head>().to_string(std::declval<Tail>()...));
        static void test_member(...);
        template <typename... Args>
        static auto test_free(Args&&...) noexcept
            -> decltype(up_adl_to_string::invoke(std::declval<Args>()...));
        static void test_free(...);
    public:
        template <typename... Args>
        static constexpr auto member() -> bool
        {
            return noexcept(test_member(std::declval<Args>()...))
                || !noexcept(test_free(std::declval<Args>()...));
        }
    };


    /**
     * Overloaded function template for invoking a member to_string function.
     * It will only take part in overload resolution if there is matching
     * member function.
     */
    template <typename Head, typename... Tail>
    auto invoke_to_string(Head&& head, Tail&&... tail)
        -> typename std::enable_if<
            invoke_to_string_aux::member<Head, Tail...>(),
            decltype(std::declval<Head>().to_string(std::declval<Tail>()...))
            >::type
    {
        return std::forward<Head>(head).to_string(std::forward<Tail>(tail)...);
    }

    /**
     * Overloaded function template for invoking a non-member to_string
     * function. It will only take part in overload resolution if there is
     * matching non-member function.
     */
    template <typename... Args>
    auto invoke_to_string(Args&&... args)
        -> typename std::enable_if<
            !invoke_to_string_aux::member<Args...>(),
            decltype(up_adl_to_string::invoke(std::declval<Args>()...))
            >::type
    {
        return up_adl_to_string::invoke(std::forward<Args>(args)...);
    }

}

namespace up
{

    using up_to_string::invoke_to_string;

}
