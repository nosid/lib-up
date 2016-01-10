#pragma once

#include "up_string_literal.hpp"

namespace up_source_location
{

    /**
     * This class is intended to be used to store the location within a source
     * file--for debugging and similar purposes. Minimum overhead is an
     * important design goal.
     */
    class source_location final
    {
    private: // --- scope ---
        enum class internal { };
    private: // --- state ---
        const char* _file;
        int _line;
    public: // --- life ---
        source_location(internal = {}, const char* file = INVOCATION_FILE(), int line = INVOCATION_LINE())
            : _file(file), _line(line)
        { }
    public: // --- operations ---
        void out(std::ostream& os) const;
    };

}

namespace up
{

    using up_source_location::source_location;

}
