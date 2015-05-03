#include "up_terminate.hpp"

#include <iostream>

#include "up_out.hpp"
#include "up_utility.hpp"

namespace
{

    class dump final
    {
    public: // --- state ---
        const up::fabric& _fabric;
    public: // --- operations ---
        void out(std::ostream& os) const
        {
            up::out(os, up::type_display_name(_fabric.type_info()), ':', _fabric.value());
            auto p = _fabric.details().begin();
            auto q = _fabric.details().end();
            if (p != q) {
                up::out(os, '{');
                up::out(os, dump{*p});
                for (++p; p != q; ++p) {
                    up::out(os, ',', dump{*p});
                }
                up::out(os, '}');
            }
        }
    };

}

void up_terminate::terminate_aux(
    up::source_location location, up::string_literal message, const up::fabrics& details)
{
    up::out(std::clog, "TERMINATE: ", message, '\n');
    up::out(std::clog, location, '\n');
    for (auto&& detail : details) {
        up::out(std::clog, '\t', dump{detail}, '\n');
    }
    up::context_frame_walk(
        [](const up::source_location& location, const up::fabrics& details) {
            up::out(std::clog, location, '\n');
            for (auto&& detail : details) {
                up::out(std::clog, '\t', dump{detail}, '\n');
            }
        });
    std::clog << std::flush;
    std::terminate();
}
