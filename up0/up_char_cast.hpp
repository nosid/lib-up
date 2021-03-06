#pragma once

namespace up_char_cast
{

    template <typename Type>
    struct is_any_char final : std::false_type { };

    template <>
    struct is_any_char<char> final : std::true_type { };

    template <>
    struct is_any_char<unsigned char> final : std::true_type { };

    template <>
    struct is_any_char<signed char> final : std::true_type { };

    template <typename Target, typename Source>
    auto char_cast(Source* value)
    {
        static_assert(is_any_char<Source>{} || std::is_void<Source>{});
        static_assert(is_any_char<Target>{});
        return static_cast<Target*>(static_cast<void*>(value));
    }

    template <typename Target, typename Source>
    auto char_cast(const Source* value)
    {
        static_assert(is_any_char<Source>{});
        static_assert(is_any_char<Target>{});
        return static_cast<const Target*>(static_cast<const void*>(value));
    }

}

namespace up
{

    using up_char_cast::char_cast;

}
