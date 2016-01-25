#pragma once

/**
 * This file simplifies the implementation of output functions. Instead of
 * overloading the stream operator, a class can provide a member or non-member
 * function named 'out'. However, it will only be used, if 'up::out' is used
 * to write the arguments to the std::ostream.
 */

#include "up_detection_idiom.hpp"

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

    template <typename Stream, typename Type>
    using member_out_t = decltype(std::declval<Type>().out(std::declval<Stream>()));

    template <typename Stream, typename Type>
    using has_member_out = up::is_detected<member_out_t, Stream, Type>;

    template <typename Stream, typename Type>
    using free_out_t = decltype(up_adl_out::invoke(std::declval<Stream>(), std::declval<Type>()));

    template <typename Stream, typename Type>
    using has_free_out = up::is_detected<free_out_t, Stream, Type>;


    /**
     * Overloaded function template for invoking a member 'out' function. It
     * will only take part in overload resolution if there is matching member
     * function.
     */
    template <typename Stream, typename Type>
    auto invoke_out(Stream&& os, Type&& value)
        -> std::enable_if_t<
            has_member_out<Stream, Type>::value,
            decltype(std::declval<Type>().out(std::declval<Stream>()))>
    {
        return std::forward<Type>(value).out(std::forward<Stream>(os));
    }

    /**
     * Overloaded function template for invoking a non-member 'out' function.
     * It will only take part in overload resolution if there is matching
     * non-member function.
     */
    template <typename Stream, typename Type>
    auto invoke_out(Stream&& os, Type&& value)
        -> std::enable_if_t<
            !has_member_out<Stream, Type>() && has_free_out<Stream, Type>(),
            decltype(up_adl_out::invoke(std::declval<Stream>(), std::declval<Type>()))>
    {
        return up_adl_out::invoke(std::forward<Stream>(os), std::forward<Type>(value));
    }

    /**
     * Overloaded function template for using stream operators instead of the
     * 'out' function. It will only take part in overload resolution if there
     * is neither a matching member nor non-member 'out' function.
     */
    template <typename Stream, typename Type>
    auto invoke_out(Stream&& os, Type&& value)
        -> std::enable_if_t<
            !has_member_out<Stream, Type>() && !has_free_out<Stream, Type>(),
            decltype(std::declval<Stream>() << std::declval<Type>())>
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
