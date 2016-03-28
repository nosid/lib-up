#include "up_chrono.hpp"

#include <iomanip>
#include <sstream>

#include "up_out.hpp"

auto up_chrono::to_string(const system_time_point& value) -> up::unique_string
{
    return to_string(value.time_since_epoch());
}

auto up_chrono::to_string(const steady_time_point& value) -> up::unique_string
{
    return to_string(value.time_since_epoch());
}

auto up_chrono::to_string(const duration& value) -> up::unique_string
{
    auto s = std::chrono::duration_cast<std::chrono::seconds>(value);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(value - s);
    std::ostringstream os;
    up::out(os, std::setfill('0'), s.count(), '.', std::setw(3), ms.count());
    return up::to_string_view(os.str());
}
