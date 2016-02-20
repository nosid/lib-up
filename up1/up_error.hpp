#pragma once

#include "up_source.hpp"

namespace up_error
{

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
    class error
    {
    private: // --- scope ---
        using self = error;
    private: // --- state ---
        up::source _source;
    public: // --- life ---
        explicit error(up::source source) noexcept
            : _source(std::move(source))
        { }
        error(const self& rhs) noexcept = default;
        error(self&& rhs) noexcept = default;
    protected:
        /**
         * The destructor is declared virtual to add a vtable to the class,
         * that can be used in debugging tools. It is not strictly necessary
         * for throwing or catching exceptions.
         */
        virtual ~error() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & noexcept -> self& = default;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        auto source() const noexcept -> const up::source&
        {
            return _source;
        }
    };


    /**
     * Some exception classes from the standard library are not default
     * constructible. This class template is used to wrap them into a
     * constructible class if required (where "wrap into" means "inherit
     * from").
     */
    template <typename Error, bool = std::is_default_constructible<Error>::value>
    class default_constructible final
    {
    public: // --- scope ---
        using type = Error;
    };

    template <typename Error>
    class default_constructible<Error, false> final
    {
    public: // --- scope ---
        class type : public Error
        {
        public: // --- life ---
            /**
             * The empty string is passed to the base constructor, because the
             * function 'what()' will be overridden in a derived class, and
             * the empty is usually constructed without dynamic memory
             * allocation.
             */
            explicit type() noexcept
                : Error(std::string())
            { }
            using Error::Error;
        private: // --- operations ---
            /**
             * The function must be overridden in a derived class, even if it
             * has already been defined in a base class.
             */
            auto what() const noexcept -> const char* override = 0;
        };
    };

    extern template class default_constructible<std::exception>;
    extern template class default_constructible<std::bad_exception>;
    extern template class default_constructible<std::logic_error>;
    extern template class default_constructible<std::domain_error>;
    extern template class default_constructible<std::invalid_argument>;
    extern template class default_constructible<std::length_error>;
    extern template class default_constructible<std::out_of_range>;
    extern template class default_constructible<std::runtime_error>;
    extern template class default_constructible<std::range_error>;
    extern template class default_constructible<std::overflow_error>;
    extern template class default_constructible<std::underflow_error>;

    template <typename Error>
    using default_constructible_t = typename default_constructible<Error>::type;


    template <typename Derived>
    class catchable { };


    /**
     * Use multiple inheritance for exception classes, so that they are
     * derived from up::error, directly or indirectly from std::exception, and
     * from an arbitrary number of further classes catch specific error
     * conditions (with minimal boiler-plate for defining those classes).
     */
    template <typename Error, typename... Catchables>
    class unified : public error, public default_constructible_t<Error>, public Catchables...
    {
    public: // --- life ---
        template <typename... Args>
        explicit unified(up::source source, Args&&... args) noexcept
            : error(std::move(source)), default_constructible_t<Error>(), Catchables(std::forward<Args>(args))...
        {
            static_assert(std::is_base_of<std::exception, Error>::value, "must be derived from std::exception");
        }
    private: // --- operations ---
        auto what() const noexcept -> const char* override final
        {
            return error::source().label_c_str();
        }
    };

    extern template class unified<std::exception>;
    extern template class unified<std::bad_exception>;
    extern template class unified<std::logic_error>;
    extern template class unified<std::domain_error>;
    extern template class unified<std::invalid_argument>;
    extern template class unified<std::length_error>;
    extern template class unified<std::out_of_range>;
    extern template class unified<std::runtime_error>;
    extern template class unified<std::range_error>;
    extern template class unified<std::overflow_error>;
    extern template class unified<std::underflow_error>;


    template <typename Catchable>
    using enable_if_catchable_t = std::enable_if_t<
        std::is_base_of<catchable<Catchable>, Catchable>::value
        && !std::is_base_of<std::exception, Catchable>::value,
        Catchable>;

    template <typename Error, typename... Catchables>
    using make_error_result_t = unified<
        std::enable_if_t<std::is_base_of<std::exception, Error>::value, Error>,
        enable_if_catchable_t<std::decay_t<Catchables>>...>;

    template <typename Error = std::exception, typename..., typename... Catchables>
    auto make_error(up::source source, Catchables&&... catchables)
        -> make_error_result_t<Error, Catchables...>
    {
        return make_error_result_t<Error, Catchables...>(
            std::move(source), std::forward<Catchables>(catchables)...);
    }

    template <typename Error = std::exception, typename..., typename... Catchables>
    [[noreturn]]
    void throw_error(up::source source, Catchables&&... catchables)
    {
        throw make_error<Error, Catchables...>(
            std::move(source), std::forward<Catchables>(catchables)...);
    }

}

namespace up
{

    using up_error::error;
    using up_error::catchable;
    using up_error::make_error;
    using up_error::throw_error;

}
