#pragma once

/**
 * @file
 *
 * @brief Header file to ease the usage and the implementation of @c swap
 * functions (both for member and free functions).
 */

#include "up_detection_idiom.hpp"

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

    template <typename Lhs, typename Rhs>
    using member_swap_t = decltype(std::declval<Lhs>().swap(std::declval<Rhs>()));

    template <typename Lhs, typename Rhs>
    using has_member_swap = up::is_detected<member_swap_t, Lhs, Rhs>;

    template <typename Lhs, typename Rhs>
    using free_swap_t = decltype(up_adl_swap::invoke(std::declval<Lhs>(), std::declval<Rhs>()));

    template <typename Lhs, typename Rhs>
    using has_free_swap = up::is_detected<free_swap_t, Lhs, Rhs>;


    /**
     * @private
     *
     * Overloaded function template for invoking a member 'swap' function. It
     * will only take part in overload resolution if there is matching member
     * function.
     */
    template <typename Lhs, typename Rhs>
    auto invoke_swap(Lhs&& lhs, Rhs&& rhs)
        noexcept(noexcept(std::declval<Lhs>().swap(std::declval<Rhs>())))
        -> std::enable_if_t<
            has_member_swap<Lhs, Rhs>() || !has_free_swap<Lhs, Rhs>(),
            decltype(std::declval<Lhs>().swap(std::declval<Rhs>()))>
    {
        return std::forward<Lhs>(lhs).swap(std::forward<Rhs>(rhs));
    }

    /**
     * @private
     *
     * Overloaded function template for invoking a non-member 'swap' function.
     * It will only take part in overload resolution if there is no matching
     * member function.
     */
    template <typename Lhs, typename Rhs>
    auto invoke_swap(Lhs&& lhs, Rhs&& rhs)
        noexcept(noexcept(up_adl_swap::invoke(std::declval<Lhs>(), std::declval<Rhs>())))
        -> std::enable_if_t<
            !has_member_swap<Lhs, Rhs>() && has_free_swap<Lhs, Rhs>(),
            decltype(up_adl_swap::invoke(std::declval<Lhs>(), std::declval<Rhs>()))>
    {
        return up_adl_swap::invoke(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs));
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
