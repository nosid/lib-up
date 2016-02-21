#include "up_istring.hpp"

#include <cstring>

#include "up_char_cast.hpp"
#include "up_exception.hpp"
#include "up_ints.hpp"

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
     * Index 1-7: Contains at most the first seven characters of the string,
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
     * seven (or six) characters are cached inline. There is also a need for a
     * dirty flag, because the cache might get outdated. */

    struct runtime;

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


    // TODO: optimize all comparison functions with inline prefix

    auto compare_aux(
        const char* ldata, std::size_t lpos, std::size_t ln,
        const char* rdata, std::size_t rpos, std::size_t rn) -> int
    {
        return std::lexicographical_compare(
            ldata + lpos, ldata + lpos + ln,
            rdata + rpos, rdata + rpos + rn);
    }

    auto tail_n(std::size_t size, std::size_t pos) -> std::size_t
    {
        if (pos > size) {
            throw up::make_throwable<std::out_of_range>("up-istring-compare");
        } else {
            return size - pos;
        }
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
        char* temp = up::char_cast<char>(::operator new(size));
        if (temp == nullptr) {
            up::raise<runtime>("istring-out-of-memory", size);
        } else {
            static_assert(std::is_trivial<char*>::value, "requires trivial type");
            new (_core + core_size / 2) char*{temp};
            std::memcpy(temp, data, size);
        }
    } else {
        using sizes = up::ints::domain<std::size_t>::or_length_error;
        auto total = sizes::add(size, sizeof(size));
        _core[0] = uchar_max();
        std::memcpy(_core + 1, data, core_size / 2 - 1);
        char* temp = up::char_cast<char>(::operator new(total));
        if (temp == nullptr) {
            up::raise<runtime>("istring-out-of-memory", size);
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

up_istring::istring::istring(up::string_view string)
    : istring(string.data(), string.size())
{ }

up_istring::istring::istring(up::chunk::from chunk)
    : istring(chunk.data(), chunk.size())
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
        ::operator delete(cast_ptr(_core), size());
    } else {
        // long string
        ::operator delete(cast_ptr(_core), size());
    }
}

auto up_istring::istring::operator=(const self& rhs) & -> self&
{
    self(rhs).swap(*this);
    return *this;
}

auto up_istring::istring::operator=(self&& rhs) & noexcept -> self&
{
    swap(rhs);
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

auto up_istring::istring::compare(const self& rhs) const noexcept -> int
{
    return compare_aux(
        data(), 0, size(),
        rhs.data(), 0, rhs.size());
}

auto up_istring::istring::compare(std::size_t lhs_pos, std::size_t lhs_n, const self& rhs) const -> int
{
    return compare_aux(
        data(), lhs_pos, std::min(lhs_n, tail_n(size(), lhs_pos)),
        rhs.data(), 0, rhs.size());
}

auto up_istring::istring::compare(std::size_t lhs_pos, std::size_t lhs_n, const self& rhs, std::size_t rhs_pos, std::size_t rhs_n) const -> int
{
    return compare_aux(
        data(), lhs_pos, std::min(lhs_n, tail_n(size(), lhs_pos)),
        rhs.data(), rhs_pos, std::min(rhs_n, tail_n(rhs.size(), rhs_pos)));
}

auto up_istring::istring::compare(const char* rhs) const -> int
{
    return compare_aux(
        data(), 0, size(),
        rhs, 0, std::strlen(rhs));
}

auto up_istring::istring::compare(std::size_t lhs_pos, std::size_t lhs_n, const char* rhs) const -> int
{
    return compare_aux(
        data(), lhs_pos, std::min(lhs_n, tail_n(size(), lhs_pos)),
        rhs, 0, std::strlen(rhs));
}

auto up_istring::istring::compare(std::size_t lhs_pos, std::size_t lhs_n, const char* rhs, std::size_t rhs_n) const -> int
{
    return compare_aux(
        data(), lhs_pos, std::min(lhs_n, tail_n(size(), lhs_pos)),
        rhs, 0, rhs_n);
}

up_istring::istring::operator up::string_view() const
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
