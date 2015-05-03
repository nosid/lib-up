#include "up_istring.hpp"

#include <cstring>

#include "up_char_cast.hpp"
#include "up_exception.hpp"

namespace
{

    /*
     * Core layout (e.g. for N=16):
     *
     * Index 0: Tag
     *
     * - 0-15 (short):    length of string, string is stored inline
     * - 16-254 (medium): length of string, string is stored outline (without size)
     * - 255 (long):      string and size are stored outline
     *
     * Index 1-7: Contains almost the first seven characters of the string,
     *     regardless whether it is stored inline or outline. The prefix can
     *     be used to optimize comparison also for medium and long strings.
     *
     * Index 8-15:
     *
     * - short:  Used for remaining characters
     * - medium: pointer to string
     * - long:   pointer to size followed by string
     */

    /* TODO: Why not supporting modifications? For strings <16, the capacity
     * is always 15. For strings with capacity 16-254, the length is stored in
     * the second field, followed by the first 6 characters. For larger
     * capacities, the size and capacity are stored outline and the first
     * seven (or six) characters inline. */

    struct runtime;
    struct out_of_range;

    constexpr auto uchar_max()
    {
        return std::numeric_limits<unsigned char>::max();
    }

    auto cast_data(const unsigned char* core)
    {
        return up::char_cast<char>(core + 1);
    }

    auto cast_ptr(const unsigned char* core) -> auto&
    {
        return *static_cast<char*const*>(static_cast<const void*>(core + sizeof(char*)));
    }

}


up_istring::istring::istring()
{
    static_assert(sizeof(self) == core_size, "consistency check");
    std::memset(_core, 0, core_size);
}

up_istring::istring::istring(const char* data, std::size_t size)
{
    if (size < core_size) {
        _core[0] = size;
        std::memcpy(_core + 1, data, size);
    } else if (size < uchar_max()) {
        _core[0] = size;
        std::memcpy(_core + 1, data, core_size / 2 - 1);
        char* temp = up::char_cast<char>(std::malloc(size));
        if (temp == nullptr) {
            UP_RAISE(runtime, "istring-out-of-memory"_s, size);
        } else {
            static_assert(std::is_trivial<char*>::value, "requires trivial type");
            new (_core + core_size / 2) char*{temp};
            std::memcpy(temp, data, size);
        }
    } else if (size + sizeof(size) < size) {
        UP_RAISE(out_of_range, "istring-overflow"_s, size);
    } else {
        _core[0] = uchar_max();
        std::memcpy(_core + 1, data, core_size / 2 - 1);
        char* temp = up::char_cast<char>(std::malloc(size + sizeof(size)));
        if (temp == nullptr) {
            UP_RAISE(runtime, "istring-out-of-memory"_s, size);
        } else {
            static_assert(std::is_trivial<char*>::value, "requires trivial type");
            static_assert(std::is_trivial<std::size_t>::value, "requires trivial type");
            new (_core + core_size / 2) char*{temp};
            new (temp) std::size_t(size);
            std::memcpy(temp + sizeof(size), data, size);
        }
    }
}

up_istring::istring::istring(const char* data)
    : istring(data, std::strlen(data))
{ }

up_istring::istring::istring(up::chunk::from chunk)
    : istring(chunk.data(), chunk.size())
{ }

up_istring::istring::istring(const std::string& string)
    : istring(string.data(), string.size())
{ }

up_istring::istring::istring(const self& rhs)
    : istring(rhs.data(), rhs.size())
{ }

up_istring::istring::istring(self&& rhs) noexcept
    : istring()
{
    std::swap(_core, rhs._core);
}

up_istring::istring::~istring() noexcept
{
    if (_core[0] < core_size) {
        // short string, nothing to do
    } else if (_core[0] < uchar_max()) {
        // medium string
        std::free(cast_ptr(_core));
    } else {
        // long string
        std::free(cast_ptr(_core));
    }
}

auto up_istring::istring::operator=(const self& rhs) & -> self&
{
    istring(rhs).swap(*this);
    return *this;
}

auto up_istring::istring::operator=(self&& rhs) & noexcept -> self&
{
    std::swap(_core, rhs._core);
    return *this;
}

void up_istring::istring::swap(self& rhs) noexcept
{
    std::swap(_core, rhs._core);
}

auto up_istring::istring::size() const -> std::size_t
{
    if (_core[0] < uchar_max()) {
        return _core[0];
    } else {
        std::size_t result;
        std::memcpy(&result, cast_ptr(_core), sizeof(result));
        return result;
    }
}

auto up_istring::istring::data() const -> const char*
{
    if (_core[0] < core_size) {
        // short string
        return cast_data(_core);
    } else if (_core[0] < uchar_max()) {
        // medium string
        return cast_ptr(_core);
    } else {
        // long string
        return cast_ptr(_core) + sizeof(std::size_t);
    }
}

up_istring::istring::operator up::chunk::from() const
{
    return {data(), size()};
}

auto up_istring::istring::to_string() const -> std::string
{
    return std::string(data(), size());
}

void up_istring::istring::out(std::ostream& os) const
{
    os.write(data(), size());
}
