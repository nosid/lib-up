#include "up_optional_string.hpp"


bool up_optional_string::operator==(const optional_string& lhs, const optional_string& rhs) noexcept
{
    if (!lhs) {
        return !static_cast<bool>(rhs);
    } else if (!rhs) {
        return false;
    } else {
        return *lhs == *rhs;
    }
}

bool up_optional_string::operator!=(const optional_string& lhs, const optional_string& rhs) noexcept
{
    return !(lhs == rhs);
}

bool up_optional_string::operator<(const optional_string& lhs, const optional_string& rhs) noexcept
{
    if (!lhs) {
        return static_cast<bool>(rhs);
    } else if (!rhs) {
        return false;
    } else {
        return *lhs < *rhs;
    }
}

bool up_optional_string::operator>(const optional_string& lhs, const optional_string& rhs) noexcept
{
    return rhs < lhs;
}

bool up_optional_string::operator<=(const optional_string& lhs, const optional_string& rhs) noexcept
{
    return !(rhs < lhs);
}

bool up_optional_string::operator>=(const optional_string& lhs, const optional_string& rhs) noexcept
{
    return !(lhs < rhs);
}

bool up_optional_string::operator==(const optional_string& lhs, up::nullopt_t) noexcept
{
    return !static_cast<bool>(lhs);
}

bool up_optional_string::operator==(up::nullopt_t, const optional_string& rhs) noexcept
{
    return !static_cast<bool>(rhs);
}

bool up_optional_string::operator!=(const optional_string& lhs, up::nullopt_t) noexcept
{
    return static_cast<bool>(lhs);
}

bool up_optional_string::operator!=(up::nullopt_t, const optional_string& rhs) noexcept
{
    return static_cast<bool>(rhs);
}

bool up_optional_string::operator<(const optional_string& lhs __attribute__((unused)), up::nullopt_t) noexcept
{
    return false;
}

bool up_optional_string::operator<(up::nullopt_t, const optional_string& rhs) noexcept
{
    return static_cast<bool>(rhs);
}

bool up_optional_string::operator>(const optional_string& lhs, up::nullopt_t) noexcept
{
    return static_cast<bool>(lhs);
}

bool up_optional_string::operator>(up::nullopt_t, const optional_string& rhs __attribute__((unused))) noexcept
{
    return false;
}

bool up_optional_string::operator<=(const optional_string& lhs, up::nullopt_t) noexcept
{
    return !static_cast<bool>(lhs);
}

bool up_optional_string::operator<=(up::nullopt_t, const optional_string& rhs __attribute__((unused))) noexcept
{
    return true;
}

bool up_optional_string::operator>=(const optional_string& lhs __attribute__((unused)), up::nullopt_t) noexcept
{
    return true;
}

bool up_optional_string::operator>=(up::nullopt_t, const optional_string& rhs) noexcept
{
    return !static_cast<bool>(rhs);
}

bool up_optional_string::operator==(const optional_string& lhs, const up::string_view& rhs)
{
    return lhs && *lhs == rhs;
}

bool up_optional_string::operator==(const up::string_view& lhs, const optional_string& rhs)
{
    return rhs && lhs == *rhs;
}

bool up_optional_string::operator!=(const optional_string& lhs, const up::string_view& rhs)
{
    return !(lhs == rhs);
}

bool up_optional_string::operator!=(const up::string_view& lhs, const optional_string& rhs)
{
    return !(lhs == rhs);
}

bool up_optional_string::operator<(const optional_string& lhs, const up::string_view& rhs)
{
    return !lhs || *lhs < rhs;
}

bool up_optional_string::operator<(const up::string_view& lhs, const optional_string& rhs)
{
    return rhs && lhs < *rhs;
}

bool up_optional_string::operator>(const optional_string& lhs, const up::string_view& rhs)
{
    return rhs < lhs;
}

bool up_optional_string::operator>(const up::string_view& lhs, const optional_string& rhs)
{
    return rhs < lhs;
}

bool up_optional_string::operator<=(const optional_string& lhs, const up::string_view& rhs)
{
    return !(rhs < lhs);
}

bool up_optional_string::operator<=(const up::string_view& lhs, const optional_string& rhs)
{
    return !(rhs < lhs);
}

bool up_optional_string::operator>=(const optional_string& lhs, const up::string_view& rhs)
{
    return !(lhs < rhs);
}

bool up_optional_string::operator>=(const up::string_view& lhs, const optional_string& rhs)
{
    return !(lhs < rhs);
}


auto std::hash<up_optional_string::optional_string>::operator()(
    const up_optional_string::optional_string& value) const noexcept -> result_type
{
    if (value) {
        auto&& repr = value.repr();
        return hash<up::string_view>()(up::string_view(repr.data(), repr.size()));
    } else {
        return 0;
    }
}
