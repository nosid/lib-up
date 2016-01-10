#include "up_test.hpp"

#include <iostream>
#include <mutex>
#include <vector>

#include "up_out.hpp"
#include "up_utility.hpp"


namespace
{

    class runner final
    {
    private: // --- scope ---
        using self = runner;
        using location = up_test::location;
        using test = up_test::test_case_base;
        static thread_local self* top;
    public:
        static void check_passed(
            location location __attribute__((unused)),
            up::string_literal type __attribute__((unused)))
        {
            ++top->_passed;
        }
        static void check_failed(location location, up::string_literal type, up::fabrics fabrics)
        {
            ++top->_failed;
            up::out(std::cerr, location.file(), ':', location.line(), ": failure: ", type, "\n");
            for (auto&& fabric : fabrics) {
                up::out(std::cerr, fabric, '\n');
            }
        }
    private: // --- state ---
        self* _parent;
        test& _test;
        std::size_t _passed = 0;
        std::size_t _failed = 0;
    public: // --- life ---
        explicit runner(test& test)
            : _parent(top), _test(test)
        {
            top = this;
        }
        runner(const self& rhs) = delete;
        runner(self&& rhs) noexcept = delete;
        ~runner() noexcept
        {
            top = _parent;
        }
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
        bool operator()() &&
        {
            try {
                _test.run();
            } catch (...) {
                ++_failed;
            }
            return _failed == 0;
        }
    };

    thread_local runner* runner::top = nullptr;


    class process final
    {
    public: // --- scope ---
        using self = process;
        using test = up_test::test_case_base;
        static auto instance() -> auto&
        {
            static self instance;
            return instance;
        }
    private: // --- state ---
        std::mutex _mutex;
        std::vector<test*> _tests;
    private: // --- life ---
        explicit process() = default;
        process(const self& rhs) = delete;
        process(self&& rhs) noexcept = delete;
        ~process() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
        void put(test* test)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _tests.push_back(test);
        }
        auto main(int argc, char* argv[]) const -> int
        {
            up::out(std::cout, "TESTS:");
            for (int i = 0; i < argc; ++i) {
                up::out(std::cout, ' ', argv[i]);
            }
            std::cout << std::endl;
            std::size_t failed = 0;
            for (auto&& test : _tests) {
                if (!runner{*test}()) {
                    ++failed;
                }
            }
            return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    };

}


up_test::test_case_base::test_case_base(location location)
    : _location(location), _armed(true)
{
    process::instance().put(this);
}

up_test::test_case_base::test_case_base(self&& rhs) noexcept
    : _location(rhs._location), _armed(rhs._armed)
{
    rhs._armed = false;
}

void up_test::test_case_base::run()
{
    up::out(std::cout, "TEST[", _location.file(), ':', _location.line(), "]: ...\n");
    _run();
}

void up_test::test_case_base::_vtable_dummy() const { }


void up_test::check::_passed(location location, up::string_literal type)
{
    runner::check_passed(location, type);
}

void up_test::check::_failed(location location, up::string_literal type, up::fabrics fabrics)
{
    runner::check_failed(location, type, std::move(fabrics));
}


auto up_test::test::main(int argc, char* argv[]) -> int
{
    return process::instance().main(argc, argv);
}
