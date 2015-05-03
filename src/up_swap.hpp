#pragma once

#include "up_include.hpp"

/**
 * The following namespace is used to find and invoke the correct overloaded,
 * non-member function 'swap'. The candidate set will include functions found
 * via ADL as well as std::swap.
 *
 * Using a dedicated namespace reduces the candidate set. It simplifies error
 * messages and avoids triggering an unintended conversion operation.
 */
namespace up_adl_swap
{

    using std::swap;

    template <typename... Args>
    auto invoke(Args&&... args)
        noexcept(noexcept(swap(std::declval<Args>()...)))
        -> decltype(swap(std::declval<Args>()...))
    {
        /* The following function invocation (intentionally) supports ADL, so
         * that 'swap' functions can be "overloaded" in the namespaces
         * corresponding to the types. */
        return swap(std::forward<Args>(args)...);
    }

}

namespace up_swap
{

    /**
     * Internal helper class to check if a swap member function exists, that
     * can be invoked with the given arguments (using SFINAE). This class is
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
            return noexcept(test_member(std::declval<Args>()...))
                || !noexcept(test_free(std::declval<Args>()...));
        }
    };


    /**
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


    inline void invoke_swap(std::string& lhs, std::string& rhs) noexcept
    {
        /* Apparently the following function is not declared as noexcept,
         * for no apparent reason. */
        lhs.swap(rhs);
    }

    template <typename... Types>
    void invoke_swap(std::unique_ptr<Types...>& lhs, std::unique_ptr<Types...>& rhs) noexcept
    {
        /* Workaround for Clang: The following function is not declared as
         * noexcept, for no apparent reason. */
        lhs.swap(rhs);
    }


    /**
     * This function should be used instead of calling 'swap' directly. In
     * addition to 'swap', it checks at compile-time whether the invoked swap
     * function is declared as noexcept.
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

    using up_swap::invoke_swap;
    using up_swap::swap_noexcept;

}
