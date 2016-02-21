#pragma once

#include "up_source.hpp"

namespace up_throwable
{

    template <typename Exception>
    using exception_t = std::enable_if_t<std::is_base_of<std::exception, Exception>::value, Exception>;


    /**
     * Some exception classes from the standard library are not default
     * constructible. This class template is used to wrap them into a
     * constructible class if required (where "wrap into" means "inherit
     * from").
     */
    template <typename Exception, bool = std::is_default_constructible<Exception>::value>
    class exception_adapter final
    {
    public: // --- scope ---
        using type = exception_t<Exception>;
    };

    template <typename Exception>
    class exception_adapter<Exception, false> final
    {
    public: // --- scope ---
        class type : public exception_t<Exception>
        {
        private: // --- scope ---
            using base = exception_t<Exception>;
        public: // --- life ---
            /**
             * The empty string is passed to the base constructor, because the
             * function 'what()' will be overridden in a derived class, and
             * the empty is usually constructed without dynamic memory
             * allocation.
             */
            explicit type() noexcept
                : base(std::string())
            { }
            using base::base;
        private: // --- operations ---
            /**
             * The function must be overridden in a derived class, even if it
             * has already been defined in a base class.
             */
            auto what() const noexcept -> const char* override = 0;
        };
    };

    extern template class exception_adapter<std::exception>;
    extern template class exception_adapter<std::bad_exception>;
    extern template class exception_adapter<std::logic_error>;
    extern template class exception_adapter<std::domain_error>;
    extern template class exception_adapter<std::invalid_argument>;
    extern template class exception_adapter<std::length_error>;
    extern template class exception_adapter<std::out_of_range>;
    extern template class exception_adapter<std::runtime_error>;
    extern template class exception_adapter<std::range_error>;
    extern template class exception_adapter<std::overflow_error>;
    extern template class exception_adapter<std::underflow_error>;

    template <typename Exception>
    using exception_adapter_t = typename exception_adapter<Exception>::type;


    /**
     * Core exception class that can be used to catch exceptions thrown from
     * the up code base. The whole design rationale is a bit more complicated:
     *
     * (a) The core exception class should provide useful and generic
     * information (i.e. up::source), while still being fully noexcept
     * (i.e. no dynamically allocated memory).
     *
     * (b) All exceptions should also be derived from std::exception, and in
     * some cases from more specific exception classes of the standard
     * library. For this reason, this class is intentionally not derived from
     * those classes. Instead the code for creating an exception will
     * automatically mix-in std::exception.
     */
    class throwable
    {
    private: // --- scope ---
        using self = throwable;
    private: // --- state ---
        up::source _source;
    public: // --- life ---
        explicit throwable(up::source&& source) noexcept
            : _source(std::move(source))
        { }
        throwable(const self& rhs) noexcept = default;
        throwable(self&& rhs) noexcept = default;
    protected:
        /**
         * The destructor is declared virtual to add a vtable to the class,
         * that can be used in debugging tools. It is not strictly necessary
         * for throwing or catching exceptions.
         */
        virtual ~throwable() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & noexcept -> self& = default;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        auto source() const noexcept -> const up::source&
        {
            return _source;
        }
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
            : public exception_adapter_t<Exception>
            , public throwable
            , private std::tuple<Fields...> // empty base class optimization
            , public Parents...
        {
        private: // --- scope ---
            using adapter = exception_adapter_t<Exception>;
            using fields = std::tuple<Fields...>;
        public: // --- life ---
            template <typename... Args>
            explicit bundle(up::source&& s, fields&& f, Args&&... args)
                : adapter()
                , throwable(std::move(s))
                , fields(std::move(f))
                , Parents(std::forward<Args>(args))...
            { }
            template <typename... Args>
            explicit bundle(adapter&& a, throwable&& t, fields&& f, Args&&... args)
                : adapter(std::move(a))
                , throwable(std::move(t))
                , fields(std::move(f))
                , Parents(std::forward<Args>(args))...
            { }
        public: // --- operations ---
            template <typename..., typename... Args>
            auto with(Args&&... args) &&
            {
                return bundle<Fields..., std::decay_t<Args>...>(
                    std::move(static_cast<adapter&>(*this)),
                    std::move(static_cast<throwable&>(*this)),
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
                    std::move(static_cast<throwable&>(*this)),
                    std::move(static_cast<fields&>(*this)),
                    std::move(static_cast<Parents&>(*this))...,
                    std::forward<Args>(args)...);
            }
        private:
            auto what() const noexcept -> const char* override final
            {
                return throwable::source().label_c_str();
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
    auto make_throwable(up::source source, Args&&... args)
    {
        return typename hierarchy<Exception, std::decay_t<Args>...>::template bundle<>(
            std::move(source), {}, std::forward<Args>(args)...);
    }

}

namespace up
{

    using up_throwable::exception_adapter_t;
    using up_throwable::throwable;
    using up_throwable::make_throwable;

}
