#include "up_terminate.hpp"

#include <iostream>

#include "up_out.hpp"
#include "up_utility.hpp"


void up_terminate::terminate_aux(const up::source& source, const up::fabrics& details)
{
    up::out(std::clog, "TERMINATE: ", source.label(), '\n');
    up::out(std::clog, source.file_c_str(), ':', source.line(), '\n');
    for (auto&& detail : details) {
        up::out(std::clog, '\t', detail, '\n');
    }
    up::context_frame_walk(
        [](const up::source& source, const up::fabrics& details) {
            up::out(std::clog, source.file_c_str(), ':', source.line(), ": ", source.label(), '\n');
            for (auto&& detail : details) {
                up::out(std::clog, '\t', detail, '\n');
            }
        });
    std::clog << std::flush;
    std::terminate();
}
