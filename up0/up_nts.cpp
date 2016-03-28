#include "up_nts.hpp"

#include <cstring>

#include "up_exception.hpp"
#include "up_ints.hpp"


up_nts::nts::nts() noexcept
{
    std::memset(_handle._data.data(), 0, handle_size);
}

up_nts::nts::nts(const up::string_view& s)
    : nts(s.data(), s.size())
{ }

up_nts::nts::nts(const char* data, size_type size)
{
    if (size < handle_size) {
        std::memcpy(_handle._data.data(), data, size);
        std::memset(_handle._data.data() + size, 0, handle_size - size);
    } else {
        using sizes = up::ints::domain<size_type>::or_overflow_error;
        _handle._padded._ref._size = sizes::add(size, 1);
        _handle._padded._ref._data = static_cast<char*>(::operator new(_handle._padded._ref._size));
        std::memcpy(_handle._padded._ref._data, data, size);
        _handle._padded._ref._data[size] = 0;
        std::memset(_handle._padded._pad.data(), 1, handle_size - sizeof(ref));
    }
}

up_nts::nts::nts(std::nullptr_t) noexcept
{
    _handle._padded._ref._size = 0;
    _handle._padded._ref._data = nullptr;
    std::memset(_handle._padded._pad.data(), 1, handle_size - sizeof(ref));
}

up_nts::nts::~nts() noexcept
{
    if (_handle._data[handle_size - 1] && _handle._padded._ref._data) {
        ::operator delete(_handle._padded._ref._data, _handle._padded._ref._size);
    } // else: nothing
}

up_nts::nts::operator const char*() const
{
    if (_handle._data[handle_size - 1]) {
        return _handle._padded._ref._data;
    } else {
        return _handle._data.data();
    }
}
