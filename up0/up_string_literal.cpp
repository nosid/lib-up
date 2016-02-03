#include "up_string_literal.hpp"

#include <ostream>

void up_string_literal::string_literal::out(std::ostream& os) const
{
    if (_data) {
        os.write(_data, _size);
    }
}
