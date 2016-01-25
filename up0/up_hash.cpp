#include "up_hash.hpp"

#include "up_char_cast.hpp"


namespace
{

    template <int digits>
    struct traits;

    template <>
    struct traits<32>
    {
        using type = uint32_t;
        __attribute__((unused))
        static const constexpr type prime = UINT32_C(16777619);
        __attribute__((unused))
        static const constexpr type offset = UINT32_C(2166136261);
    };

    template <>
    struct traits<64>
    {
        using type = uint64_t;
        __attribute__((unused))
        static const constexpr type prime = UINT64_C(1099511628211);
        __attribute__((unused))
        static const constexpr type offset = UINT64_C(14695981039346656037);
    };

    template <int digits>
    auto do_fnv1a(const unsigned char* data, std::size_t size) noexcept
    {
        typename traits<digits>::type result = traits<digits>::offset;
        for (std::size_t i = 0; i != size; ++i) {
            result ^= static_cast<typename traits<digits>::type>(data[i]);
            result *= traits<digits>::prime;
        }
        return result;
    }

}


auto up_hash::fnv1a(const unsigned char* data, std::size_t size) noexcept -> std::size_t
{
    return do_fnv1a<std::numeric_limits<std::size_t>::digits>(data, size);
}

auto up_hash::fnv1a(const char* data, std::size_t size) noexcept -> std::size_t
{
    return fnv1a(up::char_cast<unsigned char>(data), size);
}

auto up_hash::fnv1a(up::chunk::from chunk) noexcept -> std::size_t
{
    return fnv1a(chunk.data(), chunk.size());
}

auto up_hash::fnv1a(up::string_view string) noexcept -> std::size_t
{
    return fnv1a(string.data(), string.size());
}
