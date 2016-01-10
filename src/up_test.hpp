#pragma once

#include "up_fabric.hpp"
#include "up_string_literal.hpp"

/**
 * This file provides a minimalistic unit testing framework to avoid
 * dependencies on external libraries.
 */

namespace up_test
{

    class location final
    {
    private: // --- state ---
        const char* _file;
        std::size_t _line;
        const char* _func;
    public: // --- life ---
        // (intentional) implicit constructor for brace initialization
        location(const char* file, std::size_t line, const char* func)
            : _file(file), _line(line), _func(func)
        { }
    public: // --- operations ---
        auto file() const { return _file; }
        auto line() const { return _line; }
        auto func() const { return _func; }
    };


    class test_case_base
    {
    private: // --- scope ---
        using self = test_case_base;
    private: // --- state ---
        location _location;
        bool _armed;
    protected: // --- life ---
        explicit test_case_base(location location);
        test_case_base(const self& rhs) = delete;
        test_case_base(self&& rhs) noexcept;
        ~test_case_base() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
        void run();
    private:
        virtual void _run() = 0;
    private:
        // classes with vtables should have at least one out-of-line virtual method definition
        __attribute__((unused))
        virtual void _vtable_dummy() const;
    };


    template <typename Callable>
    class test_case final : private test_case_base
    {
    public: // --- scope ---
        using self = test_case;
    private: // --- state ---
        Callable _callable;
    public: // --- life ---
        explicit test_case(location location, Callable&& callable)
            : test_case_base(location), _callable(std::forward<Callable>(callable))
        { }
        test_case(const self& rhs) = delete;
        /* Note that a move constructor is required due to the way the class
         * is used in the below macro UP_TEST_CASE. */
        test_case(self&& rhs) noexcept
            : test_case_base(std::move(rhs))
            , _callable(std::forward<Callable>(rhs._callable))
        { }
        ~test_case() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
    private:
        void _run() override
        {
            _callable();
        }
    };


    class test_lambda_magic final
    {
    private: // --- state ---
        location _location;
    public: // --- life ---
        explicit test_lambda_magic(location location)
            : _location(location)
        { }
    public: // --- operations ---
        template <typename Callable>
        friend auto operator<<(test_lambda_magic&& magic, Callable&& callable)
        {
            return test_case<Callable>(magic._location, std::forward<Callable>(callable));
        }
    };


    class check final
    {
    public: // --- scope ---
        template <typename Actual, typename Expected>
        static void equal(location location, Actual&& actual, Expected&& expected)
        {
            using namespace up::literals;
            if (actual != expected || !(actual == expected)) {
                _failed_aux(location, "equal"_sl,
                    std::forward<Actual>(actual),
                    std::forward<Expected>(expected));
            } else {
                _passed(location, "equal"_sl);
            }
        }
    private:
        static void _passed(location location, up::string_literal type);
        static void _failed(location location, up::string_literal type, up::fabrics fabrics);
        template <typename... Args>
        static void _failed_aux(location location, up::string_literal type, Args&&... args)
        {
            _failed(location, type, {up::invoke_to_fabric_with_fallback(std::forward<Args>(args))...});
        }
    };


    class test final
    {
    public: // --- scope ---
        static auto main(int argc, char* argv[]) -> int;
    };

}

namespace up
{

    using up_test::test;

}


#define UP_TEST_CONCATENATE_AUX(prefix, suffix) prefix ## suffix
#define UP_TEST_CONCATENATE(prefix, suffix) UP_TEST_CONCATENATE_AUX(prefix, suffix)

#define UP_TEST_CASE \
    __attribute__((unused)) \
    auto&& UP_TEST_CONCATENATE(UP_TEST_LAMBDA_, __COUNTER__) \
        = ::up_test::test_lambda_magic({__FILE__, __LINE__, nullptr}) << []

#define UP_TEST_EQUAL(...) \
    do { \
        ::up_test::check::equal({__FILE__, __LINE__, __PRETTY_FUNCTION__}, __VA_ARGS__); \
    } while (false)
