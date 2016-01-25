#pragma once

/**
 * @file
 *
 * @brief Deferred execution of statements at the end of a scope.
 *
 * This file provides a simple way to (conditionally) execute statements at
 * the end of a scope. There are similar implementations, e.g.
 * BOOST_SCOPE_EXIT. It's primarily intended to be used, where RAII causes too
 * much boilerplate, in particular for using C-libraries with C++.
 */

/// Designated namespace for the header file.
namespace up_defer
{

    /**
     * @brief The class encapsulates the statements, which should be executed
     * at the end of the block (i.e. when the destructor is invoked).
     *
     * The class is intended to be used to build exception-safe code for
     * cases, in which RAII is not appropriate. Intentionally, it uses no
     * dynamic memory allocation, to avoid throwing exceptions by itself.
     * Instead of type erasure, type deduction has to be used for the Callable
     * type.
     */
    template <typename Callable>
    class defer final
    {
    private: // --- scope ---
        using self = defer;
    private: // --- state ---
        Callable _callable;
        bool _armed = true;
    public: // --- life ---
        /// @private
        explicit defer(Callable&& callable)
            : _callable(std::forward<Callable>(callable))
        {
            static_assert(std::is_nothrow_constructible<Callable, Callable&&>::value, "requires nothrow");
        }
        /// @private
        defer(const self& rhs) = delete;
        /**
         * @private
         *
         * Note that a move constructor is required due to the way the class
         * is used in the below macro UP_DEFER.
         */
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
    private: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
    public:
        /**
         * @brief Disarm the object, so that the statements will not be
         * executed at the end of the block (if the execution has not already
         * started).
         */
        void disarm() noexcept
        {
            _armed = false;
        }
    };

    /**
     * @private
     *
     * Unique type required for the lambda magic in @c UP_DEFER.
     */
    enum class defer_magic { };

    /**
     * @private
     *
     * Operator required for the lambda magic in @c UP_DEFER.
     */
    template <typename Callable>
    auto operator<<(defer_magic, Callable&& callable)
    {
        return defer<Callable>(std::forward<Callable>(callable));
    }

}

/// @private
#define UP_DEFER_CONCATENATE_AUX(prefix, suffix) prefix ## suffix
/// @private
#define UP_DEFER_CONCATENATE(prefix, suffix) UP_DEFER_CONCATENATE_AUX(prefix, suffix)

/**
 * @brief Syntax to declare statements for deferred executions.
 *
 * The macro should be followed by a block and a semicolon. The statements in
 * the block are executed at the end of the current block (as usual in reverse
 * order). It uses some lambda magic to achieve this syntax.
 */
#define UP_DEFER \
    __attribute__((unused)) \
    auto&& UP_DEFER_CONCATENATE(UP_DEFER_STATE_, __COUNTER__) = ::up_defer::defer_magic() << [&] () noexcept

/**
 * @brief Syntax to declare statements for deferred executions with controller
 * object.
 *
 * In addition to @c UP_DEFER, the given name is used for the controller
 * object, so that it can be referenced later in the code, e.g. to disarm the
 * deferred execution.
 */
#define UP_DEFER_NAMED(name) \
    __attribute__((unused)) \
    auto&& name = ::up_defer::defer_magic() << [&] () noexcept
