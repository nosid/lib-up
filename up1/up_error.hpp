#pragma once

#include "up_source.hpp"

namespace up_error
{

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
        ~error() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & noexcept -> self& = default;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        auto source() const noexcept -> const up::source&
        {
            return _source;
        }
    };


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
            explicit type() noexcept
                : Error(std::string())
            { }
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


    template <typename Error>
    class unified : public error, public default_constructible_t<Error>
    {
    public: // --- life ---
        explicit unified(up::source source) noexcept
            : error(std::move(source)), default_constructible_t<Error>()
        {
            static_assert(noexcept(default_constructible_t<Error>()), "requires noexcept");
        }
    private: // --- operations ---
        virtual auto what() const noexcept -> const char* override final
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


    template <typename Derived>
    class catchable { };

    template <typename Catchable>
    using enable_if_catchable_t = std::enable_if_t<
        std::is_base_of<catchable<Catchable>, Catchable>::value, Catchable>;


    template <typename Error, typename... Catchables>
    class mixed final
    {
    public: // --- scope ---
        class type : public unified<Error>, public Catchables...
        {
        public: // --- life ---
            explicit type(up::source source, Catchables... catchables)
                : unified<Error>(std::move(source)), Catchables(std::move(catchables))...
            { }
        };
    };

    template <typename Error>
    class mixed<Error> final
    {
    public: // --- scope ---
        // avoid creating a new type (with virtual destructor) in this case
        using type = unified<Error>;
    };

    template <typename Error, typename... Catchables>
    using mixed_t = typename mixed<Error, Catchables...>::type;


    template <typename Error = std::exception, typename... Catchables>
    auto make_error(up::source source, Catchables... catchables)
        -> mixed_t<Error, enable_if_catchable_t<Catchables>...>
    {
        return mixed_t<Error, Catchables...>(std::move(source), std::move(catchables)...);
    }

    template <typename Error = std::exception, typename... Catchables>
    [[noreturn]]
    void throw_error(up::source source, Catchables... catchables)
    {
        throw make_error<Error, Catchables...>(std::move(source), std::move(catchables)...);
    }

}

namespace up
{

    using up_error::error;
    using up_error::catchable;
    using up_error::make_error;
    using up_error::throw_error;

}
