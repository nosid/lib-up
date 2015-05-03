#pragma once

#include "up_string_literal.hpp"

namespace up_source_location
{

    /**
     * This class is intended to be used to store the location within a source
     * file--for debugging and similar purposes. Minimum overhead is an
     * important design goal.
     *
     * The class should not be used directly. It should only be used via the
     * macro UP_SOURCE_LOCATION.
     */
    class source_location final
    {
    private: // --- state ---
        up::string_literal _file;
        std::size_t _line;
        up::string_literal _func;
    public: // --- life ---
        template <std::size_t I, std::size_t J>
        explicit source_location(const char (&file)[I], std::size_t line, const char (&func)[J])
            : _file(up::literals::operator"" _s(file, I - 1))
            , _line(line)
            , _func(up::literals::operator"" _s(func, J - 1))
        {
            static_assert(I > 0, "unexpected size of string literal");
            static_assert(J > 0, "unexpected size of string literal");
        }
    public: // --- operations ---
        void out(std::ostream& os) const;
    };

}

namespace up
{

    using up_source_location::source_location;

}

#define UP_SOURCE_LOCATION() \
    ::up::source_location(__FILE__, __LINE__, __PRETTY_FUNCTION__)
