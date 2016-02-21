#include "up_exception.hpp"

#include <cstring>

#include "up_utility.hpp"

namespace
{

    __attribute__((unused))
    auto strerror_aux(int func(int, char*, std::size_t), int value)
    {
        std::size_t buffer_size = 256;
        for (;;) {
            auto buffer = std::make_unique<char[]>(buffer_size);
            int rv = func(value, buffer.get(), buffer_size);
            if (rv == 0) {
                return std::string(buffer.get());
            } else if (rv == ERANGE || (rv < 0 && errno == ERANGE)) {
                buffer_size *= 2;
            } else {
                return std::string("invalid-strerror");
            }
        }
    }

    __attribute__((unused))
    auto strerror_aux(char* func(int, char*, std::size_t), int value)
    {
        std::size_t buffer_size = 256;
        auto buffer = std::make_unique<char[]>(buffer_size);
        return std::string(func(value, buffer.get(), buffer_size));
    }


    void log_insight(std::ostream& os, const up::insight& insight, std::size_t depth)
    {
        up::out(os,
            std::string(depth * 4, ' '),
            up::type_display_name(insight.type_info()),
            ':',
            insight.value(),
            '\n');
        for (auto&& nested : insight.nested()) {
            log_insight(os, nested, depth + 1);
        }
    }

}


template class up_exception::hierarchy<std::exception>::bundle<>;
template class up_exception::hierarchy<std::bad_exception>::bundle<>;
template class up_exception::hierarchy<std::logic_error>::bundle<>;
template class up_exception::hierarchy<std::domain_error>::bundle<>;
template class up_exception::hierarchy<std::invalid_argument>::bundle<>;
template class up_exception::hierarchy<std::length_error>::bundle<>;
template class up_exception::hierarchy<std::out_of_range>::bundle<>;
template class up_exception::hierarchy<std::runtime_error>::bundle<>;
template class up_exception::hierarchy<std::range_error>::bundle<>;
template class up_exception::hierarchy<std::overflow_error>::bundle<>;
template class up_exception::hierarchy<std::underflow_error>::bundle<>;


auto up_exception::errno_info::to_insight() const -> up::insight
{
    return up::insight(typeid(*this), strerror_aux(::strerror_r, _value),
        up::invoke_to_insight_with_fallback(_value));
}


void up_exception::log_current_exception_aux(std::ostream& os)
{
    try {
        throw;
    } catch (const up::exception& e) {
        auto&& s = e.source();
        up::out(os, s.file(), ':', s.line(), ": ", s.label(), '\n');
        log_insight(os, e.to_insight(), 1);
    } catch (const up::throwable& e) {
        auto&& s = e.source();
        up::out(os, s.file(), ':', s.line(), ": ", s.label(), '\n');
    } catch (const std::exception& e) {
        up::out(os, e.what(), '\n');
    } catch (...) {
        up::out(os, "...\n");
    }
}


void up_exception::suppress_current_exception(const up::source& source __attribute__((unused)))
{
    /* The current exception is rethrown to make sure, that the method is
     * actually called with an active exception. */
    try {
        throw;
    } catch (...) {
        // nothing
    }
}
