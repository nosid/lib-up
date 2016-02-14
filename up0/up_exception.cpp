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


auto up_exception::errno_info::to_insight() const -> up::insight
{
    return up::insight(typeid(*this), strerror_aux(::strerror_r, _value),
        up::invoke_to_insight_with_fallback(_value));
}


void up_exception::log_current_exception_aux(std::ostream& os)
{
    try {
        throw;
    } catch (const up::exception<>& e) {
        up::out(os, e.what(), '\n');
        log_insight(os, e.to_insight(), 1);
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
