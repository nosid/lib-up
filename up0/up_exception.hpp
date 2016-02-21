#pragma once

/**
 * This file contains an exception framework for this library. The primary
 * design goals of this framework are:
 *
 * (a) provide structured information about the cause of the exception, so
 *     that the information can be logged in a unique and safe manner (and
 *     optionally in different formats),
 *
 * (b) efficient implementation and lazy serialization (pay only if it is
 *     required),
 *
 * (c) make it easy to add exception types.
 */

#include "up_insight.hpp"
#include "up_out.hpp"
#include "up_throwable.hpp"

namespace up_exception
{

    /**
     * In contrast to up::error, the class makes the attached fields
     * accessible at runtime as insights. This information should only be used
     * for logging and monitoring.
     */
    class exception : public up::throwable
    {
    private: // --- scope ---
        using self = exception;
    public: // --- life ---
        explicit exception(up::source&& source) noexcept
            : throwable(std::move(source))
        { }
        exception(const self& rhs) noexcept = default;
        exception(self&& rhs) noexcept = default;
    protected:
        virtual ~exception() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & noexcept -> self& = default;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        auto to_insight() const -> up::insight
        {
            return _to_insight();
        }
    private:
        virtual auto _to_insight() const -> up::insight = 0;
    };


    template <typename Exception, typename... Parents>
    class hierarchy final
    {
    public: // --- scope ---
        /**
         * Use multiple inheritance for exception classes, so that they are
         * derived from up::throwable, directly or indirectly from
         * std::exception, and from an arbitrary number of further classes
         * catch specific error conditions (with minimal boiler-plate for
         * defining those classes).
         */
        template <typename... Fields>
        class bundle final
            : public up::exception_adapter_t<Exception>
            , public exception
            , private std::tuple<Fields...> // empty base class optimization
            , public Parents...
        {
        private: // --- scope ---
            using adapter = up::exception_adapter_t<Exception>;
            using fields = std::tuple<Fields...>;
        public: // --- life ---
            template <typename... Args>
            explicit bundle(up::source&& s, fields&& f, Args&&... args)
                : adapter()
                , exception(std::move(s))
                , fields(std::move(f))
                , Parents(std::forward<Args>(args))...
            { }
            template <typename... Args>
            explicit bundle(adapter&& a, exception&& e, fields&& i, Args&&... args)
                : adapter(std::move(a))
                , exception(std::move(e))
                , fields(std::move(i))
                , Parents(std::forward<Args>(args))...
            { }
        public: // --- operations ---
            template <typename..., typename... Args>
            auto with(Args&&... args) &&
            {
                return bundle<Fields..., std::decay_t<Args>...>(
                    std::move(static_cast<adapter&>(*this)),
                    std::move(static_cast<exception&>(*this)),
                    std::tuple_cat(
                        std::move(static_cast<fields&>(*this)),
                        std::make_tuple(std::forward<Args>(args)...)),
                    std::move(static_cast<Parents&>(*this))...);
            }
            template <typename..., typename... Args>
            auto extends(Args&&... args) &&
            {
                return typename hierarchy<Parents..., std::decay_t<Args>...>::template bundle<Fields...>(
                    std::move(static_cast<adapter&>(*this)),
                    std::move(static_cast<exception&>(*this)),
                    std::move(static_cast<fields&>(*this)),
                    std::move(static_cast<Parents&>(*this))...,
                    std::forward<Args>(args)...);
            }
        private:
            auto what() const noexcept -> const char* override final
            {
                return throwable::source().label_c_str();
            }
            auto _to_insight() const -> up::insight override
            {
                return _to_insight_aux(std::index_sequence_for<Fields...>());
            }
            template <std::size_t... Indexes>
            auto _to_insight_aux(std::index_sequence<Indexes...>) const -> up::insight
            {
                return up::insight(
                    typeid(*this),
                    throwable::source().label(),
                    up::invoke_to_insight_with_fallback(
                        std::get<Indexes>(static_cast<const fields&>(*this)))...);
            }
        };
    };

    extern template class hierarchy<std::exception>::bundle<>;
    extern template class hierarchy<std::bad_exception>::bundle<>;
    extern template class hierarchy<std::logic_error>::bundle<>;
    extern template class hierarchy<std::domain_error>::bundle<>;
    extern template class hierarchy<std::invalid_argument>::bundle<>;
    extern template class hierarchy<std::length_error>::bundle<>;
    extern template class hierarchy<std::out_of_range>::bundle<>;
    extern template class hierarchy<std::runtime_error>::bundle<>;
    extern template class hierarchy<std::range_error>::bundle<>;
    extern template class hierarchy<std::overflow_error>::bundle<>;
    extern template class hierarchy<std::underflow_error>::bundle<>;


    template <typename Exception = std::exception, typename..., typename... Args>
    auto make_exception(up::source source, Args&&... args)
    {
        return typename hierarchy<Exception, std::decay_t<Args>...>::template bundle<>(
            std::move(source), {}, std::forward<Args>(args)...);
    }


    /**
     * The class can be used to add an error code to a raised exception. The
     * error code will be logged with a verbose error message (lazily).
     */
    class errno_info final
    {
    private: // --- state ---
        int _value;
    public: // --- life ---
        explicit errno_info(int value)
            : _value(value)
        { }
    public: // --- operations ---
        auto to_insight() const -> up::insight;
    };


    /**
     * Convenience function to log some information about the exception to the
     * given ostream.
     */
    void log_current_exception_aux(std::ostream& os);

    /**
     * Convenience function to log some information about the exception to the
     * given ostream, prefixed by the given arguments (for further
     * convenience).
     */
    template <typename... Args>
    void log_current_exception(std::ostream& os, Args&&... args)
    {
        up::out(os, std::forward<Args>(args)...);
        log_current_exception_aux(os);
    }


    /**
     * This function will be called whenever an exception is suppressed
     * because it is converted to an error code. That means the error is still
     * signaled. However, the information from the exception is lost.
     *
     * The function does basically nothing. The idea is to use this function
     * to mark all locations where exceptions are suppressed. In the future, a
     * handler for suppressed exceptions might be registered.
     */
    void suppress_current_exception(const up::source& source);

}

namespace up
{

    using up_exception::exception;
    using up_exception::make_exception;
    using up_exception::errno_info;
    using up_exception::log_current_exception;
    using up_exception::suppress_current_exception;

}
