#pragma once

/**
 * This file simplifies the implementation of output functions. Instead of
 * overloading the stream operator, a class can provide a member or non-member
 * function named 'out'. However, it will only be used, if 'up::out' is used
 * to write the arguments to the std::ostream.
 */

#include "up_include.hpp"

/**
 * The following namespace is used to find and invoke the correct overloaded,
 * non-member function 'out'. By using a dedicated namespace, only functions
 * in a namespace of one of the arguments will be searched (ADL).
 */
namespace up_adl_out
{

    template <typename... Args>
    auto invoke(Args&&... args)
        -> decltype(out(std::declval<Args>()...))
    {
        return out(std::forward<Args>(args)...);
    }

}

namespace up_out
{

    /**
     * Internal helper class to check if a 'out' member or non-member function
     * exists, that can be invoked with the given arguments (using SFINAE).
     * This class is required to choose between member and non-member
     * function.
     */
    class invoke_out_aux final
    {
    private:
        template <typename Stream, typename Head, typename... Tail>
        static auto test_member(Stream&&, Head&&, Tail&&...) noexcept
            -> decltype(std::declval<Head>().out(std::declval<Stream>(), std::declval<Tail>()...));
        static void test_member(...);
        template <typename... Args>
        static auto test_free(Args&&...) noexcept
            -> decltype(up_adl_out::invoke(std::declval<Args>()...));
        static void test_free(...);
    public:
        template <typename... Args>
        static constexpr auto member() -> bool
        {
            return noexcept(test_member(std::declval<Args>()...));
        }
        template <typename... Args>
        static constexpr auto free() -> bool
        {
            return !noexcept(test_member(std::declval<Args>()...))
                && noexcept(test_free(std::declval<Args>()...));
        }
    };


    /**
     * Overloaded function template for invoking a member 'out' function. It
     * will only take part in overload resolution if there is matching member
     * function.
     */
    template <typename Stream, typename Head, typename... Tail>
    auto invoke_out(Stream&& os, Head&& head, Tail&&... tail)
        -> typename std::enable_if<
            invoke_out_aux::member<Stream, Head, Tail...>(),
            decltype(std::declval<Head>().out(std::declval<Stream>(), std::declval<Tail>()...))
            >::type
    {
        return std::forward<Head>(head).out(
            std::forward<Stream>(os), std::forward<Tail>(tail)...);
    }

    /**
     * Overloaded function template for invoking a non-member 'out' function.
     * It will only take part in overload resolution if there is matching
     * non-member function.
     */
    template <typename Stream, typename... Args>
    auto invoke_out(Stream&& os, Args&&... args)
        -> typename std::enable_if<
            invoke_out_aux::free<Stream, Args...>(),
            decltype(up_adl_out::invoke(std::declval<Stream>(), std::declval<Args>()...))
            >::type
    {
        return up_adl_out::invoke(std::forward<Stream>(os), std::forward<Args>(args)...);
    }

    /**
     * Overloaded function template for using stream operators instead of the
     * 'out' function. It will only take part in overload resolution if there
     * is neither a matching member nor non-member 'out' function.
     */
    template <typename Stream, typename Type>
    auto invoke_out(Stream&& os, Type&& value)
        -> typename std::enable_if<
            !invoke_out_aux::member<Stream, Type>()
            && !invoke_out_aux::free<Stream, Type>(),
            decltype(std::declval<Stream>() << std::declval<Type>())
            >::type
    {
        return std::forward<Stream>(os) << std::forward<Type>(value);
    }


    // base case for variadic function below
    inline void out(std::ostream& os __attribute__((unused))) { }

    /**
     * Write the given arguments in that particular order to the given
     * ostream. In contrast to using stream operators, it has two advantages:
     * First, it recognize and use member or non-member 'out' functions.
     * Second, the syntax is more terse and causes less confusion regarding
     * operator precedence.
     */
    template <typename Head, typename... Tail>
    void out(std::ostream& os, Head&& head, Tail&&... tail)
    {
        invoke_out(os, std::forward<Head>(head));
        out(os, std::forward<Tail>(tail)...);
    }

}

namespace up
{

    using up_out::out;

}
