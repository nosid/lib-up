#pragma once

#include "up_string.hpp"

namespace up_string_literal
{

    // forward declarations (required due to friend relationship)
    class string_literal;
    constexpr auto operator "" _sl(const char* data, std::size_t size) noexcept
        -> string_literal;

    /**
     * There are two purposes for this class. First, we can distinguish
     * string literals from other const char* parameters. Second, we can use
     * the size at runtime, that is already known during compile-time.
     */
    class string_literal final
    {
    private: // --- state ---
        const char* _data = nullptr;
        std::size_t _size = 0;
    private: // --- life ---
        constexpr explicit string_literal() noexcept { }
        constexpr explicit string_literal(std::nullptr_t) noexcept { }
        constexpr explicit string_literal(const char* data, std::size_t size) noexcept
            : _data(data), _size(size)
        { }
    public: // --- operations ---
        auto data() const noexcept
        {
            return _data;
        }
        auto size() const noexcept
        {
            return _size;
        }
        auto to_string() const
        {
            return up::unique_string(_data, _size);
        }
        void out(std::ostream& os) const;
    public: // --- friends ---
        friend constexpr auto operator "" _sl(
            const char* data, std::size_t size) noexcept -> string_literal;
    };

    /**
     * String literal for string_literal. This should be the only way to
     * create (non-null) instances of string_literal.
     */
    inline constexpr auto operator "" _sl(const char* data, std::size_t size) noexcept
        -> string_literal
    {
        return string_literal(data, size);
    }

}

namespace up
{

    using up_string_literal::string_literal;

    // same convention for namespaces as in the standard library
    inline namespace literals
    {

        using up_string_literal::operator"" _sl;

    }

}
