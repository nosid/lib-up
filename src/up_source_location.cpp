#include "up_source_location.hpp"

#include "up_out.hpp"

void up_source_location::source_location::out(std::ostream& os) const
{
    up::out(os, _func, '[', _file, ':', _line, ']');
}
