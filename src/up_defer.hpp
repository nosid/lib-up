#pragma once

/**
 * This file provides a simple way to (conditionally) execute a statement at
 * the end of a block. There are similar implementations, e.g.
 * BOOST_SCOPE_EXIT. It's primarily intended to be used, where RAII causes too
 * much boilerplate, in particular for using C-libraries with C++.
 */

#include "up_include.hpp"

namespace up_defer
{

    /**
     * Class to invoke a callable (conditionally) from the destructor. It's
     * intended to be used to build exception-safe code for cases, in which
     * RAII is not appropriate. Intentionally, it uses no dynamic memory
     * allocation, to avoid throwing exceptions by itself. Instead of type
     * erase, type deduction has to be used for the Callable type.
     */
    template <typename Callable>
    class defer final
    {
    public: // --- scope ---
        using self = defer;
    private: // --- state ---
        Callable _callable;
        bool _armed = true;
    public: // --- life ---
        explicit defer(Callable&& callable)
            : _callable(std::forward<Callable>(callable))
        {
            static_assert(std::is_nothrow_constructible<Callable, Callable&&>::value, "requires nothrow");
        }
        defer(const self& rhs) = delete;
        /* Note that a move constructor is required due to the way the class
         * is used in the below macro UP_DEFER. */
        defer(self&& rhs) noexcept
            : _callable(std::forward<Callable>(rhs._callable))
            , _armed(rhs._armed)
        {
            static_assert(std::is_nothrow_constructible<Callable, Callable&&>::value, "requires nothrow");
            rhs.disarm();
        }
        ~defer() noexcept
        {
            if (_armed) {
                static_assert(noexcept(_callable()), "requires nothrow");
                _callable();
            }
        }
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
        void disarm() noexcept
        {
            _armed = false;
        }
    };

    enum class defer_magic { };

    template <typename Callable>
    auto operator<<(defer_magic, Callable&& callable)
    {
        return defer<Callable>(std::forward<Callable>(callable));
    }

}

#define UP_DEFER_CONCATENATE_AUX(prefix, suffix) prefix ## suffix
#define UP_DEFER_CONCATENATE(prefix, suffix) UP_DEFER_CONCATENATE_AUX(prefix, suffix)

#define UP_DEFER \
    __attribute__((unused)) \
    auto&& UP_DEFER_CONCATENATE(UP_DEFER_STATE_, __COUNTER__) = ::up_defer::defer_magic() << [&] () noexcept

#define UP_DEFER_NAMED(name) \
    __attribute__((unused)) \
    auto&& name = ::up_defer::defer_magic() << [&] () noexcept
