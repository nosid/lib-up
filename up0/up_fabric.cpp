#include "up_fabric.hpp"

#include "up_out.hpp"
#include "up_utility.hpp"


void up_fabric::fabric::out(std::ostream& os) const
{
    up::out(os, up::type_display_name(_type_info), ':', _value);
    auto p = _details.begin(), q = _details.end();
    if (p != q) {
        up::out(os, '{', *p);
        for (++p; p != q; ++p) {
            up::out(os, ',', *p);
        }
        up::out(os, '}');
    }
}
