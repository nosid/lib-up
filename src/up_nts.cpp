#include "up_nts.hpp"

#include <cstring>

#include "up_exception.hpp"


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
        _handle._ref._size = size + 1;
        if (_handle._ref._size < size) { // XXX:OVERFLOW
            UP_RAISE(struct runtime, "nts-overflow"_s);
        }
        _handle._ref._data = static_cast<char*>(::operator new(_handle._ref._size));
        std::memcpy(_handle._ref._data, data, size);
        _handle._ref._data[size] = 0;
        std::memset(_handle._dummy.data(), 1, handle_size - sizeof(ref));
    }
}

up_nts::nts::~nts() noexcept
{
    if (_handle._data[handle_size - 1]) {
        ::operator delete(_handle._ref._data, _handle._ref._size);
    } // else: nothing
}

up_nts::nts::operator const char*() const &&
{
    if (_handle._data[handle_size - 1]) {
        return _handle._ref._data;
    } else {
        return _handle._data.data();
    }
}
