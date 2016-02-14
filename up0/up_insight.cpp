#include "up_insight.hpp"

#include "up_out.hpp"
#include "up_utility.hpp"


void up_insight::insight::out(std::ostream& os) const
{
    up::out(os, up::type_display_name(_type_info), ':', _value);
    auto p = _nested.begin(), q = _nested.end();
    if (p != q) {
        up::out(os, '{', *p);
        for (++p; p != q; ++p) {
            up::out(os, ',', *p);
        }
        up::out(os, '}');
    }
}
