#include "up_terminate.hpp"

#include <iostream>

#include "up_out.hpp"
#include "up_utility.hpp"


void up_terminate::terminate_aux(
    up::source_location location, up::string_literal message, const up::fabrics& details)
{
    up::out(std::clog, "TERMINATE: ", message, '\n');
    up::out(std::clog, location, '\n');
    for (auto&& detail : details) {
        up::out(std::clog, '\t', detail, '\n');
    }
    up::context_frame_walk(
        [](const up::source_location& location, const up::fabrics& details) {
            up::out(std::clog, location, '\n');
            for (auto&& detail : details) {
                up::out(std::clog, '\t', detail, '\n');
            }
        });
    std::clog << std::flush;
    std::terminate();
}
