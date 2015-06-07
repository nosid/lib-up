#pragma once

/**
 * @file
 *
 * @brief Header file to ease the usage and the implementation of @c swap
 * functions (both for member and free functions).
 */

#include "up_include.hpp"

/**
 * @brief The namespace is used to find and invoke the proper non-member
 * function @c swap. The candidate set will include functions found via ADL as
 * well as @c std::swap.
 *
 * Using a dedicated/isolated namespace reduces the candidate set. It
 * simplifies error messages and avoids triggering an unintended conversion
 * operation.
 */
namespace up_adl_swap
{

    using std::swap;

    /**
     * @private
     *
     * The function template invokes the best matching function of the
     * candidate set, that consists of @c std::swap and all functions found
     * via argument dependent lookup (ADL). ADL is intentionally used, so that
     * types can provide an "overload" in their own namespace.
     */
    template <typename... Args>
    auto invoke(Args&&... args)
        noexcept(noexcept(swap(std::declval<Args>()...)))
        -> decltype(swap(std::declval<Args>()...))
    {
        return swap(std::forward<Args>(args)...);
    }

}

/// Designated namespace for the header file.
namespace up_swap
{

    /**
     * @private
     *
     * Internal helper class to check if there is a proper swap member
     * function, i.e. it can be invoked with the given arguments. The class is
     * required to choose between member and non-member function.
     */
    class invoke_swap_aux final
    {
    private:
        template <typename Head, typename... Tail>
        static auto test_member(Head&&, Tail&&...) noexcept
            -> decltype(std::declval<Head>().swap(std::declval<Tail>()...));
        static void test_member(...);
        template <typename... Args>
        static auto test_free(Args&&...) noexcept
            -> decltype(up_adl_swap::invoke(std::declval<Args>()...));
        static void test_free(...);
    public:
        template <typename... Args>
        static constexpr auto member() -> bool
        {
            /* Prefer the member function if both exist. If neither exists,
             * also prefer the member function, so that the compiler error
             * message will point to the class with the missing overload. */
            return noexcept(test_member(std::declval<Args>()...))
                || !noexcept(test_free(std::declval<Args>()...));
        }
    };


    /**
     * @private
     *
     * Overloaded function template for invoking a member 'swap' function. It
     * will only take part in overload resolution if there is matching member
     * function.
     */
    template <typename Head, typename... Tail>
    auto invoke_swap(Head&& head, Tail&&... tail)
        noexcept(noexcept(std::declval<Head>().swap(std::declval<Tail>()...)))
        -> typename std::enable_if<
            invoke_swap_aux::member<Head, Tail...>(),
            decltype(std::declval<Head>().swap(std::declval<Tail>()...))
            >::type
    {
        return std::forward<Head>(head).swap(std::forward<Tail>(tail)...);
    }

    /**
     * @private
     *
     * Overloaded function template for invoking a non-member 'swap' function.
     * It will only take part in overload resolution if there is no matching
     * member function.
     */
    template <typename... Args>
    auto invoke_swap(Args&&... args)
        noexcept(noexcept(up_adl_swap::invoke(std::declval<Args>()...)))
        -> typename std::enable_if<
            !invoke_swap_aux::member<Args...>(),
            decltype(up_adl_swap::invoke(std::declval<Args>()...))
            >::type
    {
        return up_adl_swap::invoke(std::forward<Args>(args)...);
    }


    /**
     * @private
     *
     * Apparently, the following function is neither in GCC nor Clang declared
     * as @c noexcept (for no apparent reason).
     */
    inline void invoke_swap(std::string& lhs, std::string& rhs) noexcept
    {
        /* Apparently the following function is not declared as noexcept,
         * for no apparent reason. */
        lhs.swap(rhs);
    }

    /**
     * @private
     *
     * WORKAROUND for Clang: The following function is not declared as @c
     * noexcept in libc++ (for no apparent reason).
     */
    template <typename... Types>
    void invoke_swap(std::unique_ptr<Types...>& lhs, std::unique_ptr<Types...>& rhs) noexcept
    {
        lhs.swap(rhs);
    }


    /**
     * @brief The function invokes 'swap' with several benefits compared to
     * invoking 'std::swap' directly.
     *
     * First, it supports both member and non-member @c swap functions,
     * preferring the former. Second, it checks at compile-time whether the
     * actually invoked @c swap function is declared @c noexcept. And third,
     * it avoids naming conflicts when invoking @c swap functions from the
     * implementation of a @c swap function, because this function uses
     * intentionally a different name.
     */
    template <typename... Args>
    void swap_noexcept(Args&&... args) noexcept
    {
        static_assert(noexcept(invoke_swap(std::forward<Args>(args)...)), "requires noexcept");
        invoke_swap(std::forward<Args>(args)...);
    }

}

namespace up
{

    using up_swap::swap_noexcept;

}
