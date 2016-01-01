#pragma once

/*
 * This file provides a macro to terminate the program in case of fatal
 * errors, that can not be handled by throwing an exception. Most importantly
 * if a thrown exception might (to easily) cause undefined behavior.
 */


#include "up_fabric.hpp"
#include "up_source_location.hpp"
#include "up_string_literal.hpp"

namespace up_terminate
{

    [[noreturn]]
    void terminate_aux(
        up::source_location location, up::string_literal message, const up::fabrics& details);

    template <typename... Args>
    [[noreturn]]
    void terminate(up::source_location location, up::string_literal message, Args&&... args)
    {
        terminate_aux(std::move(location), std::move(message),
            up::fabrics{up::invoke_to_fabric_with_fallback(std::forward<Args>(args))...});
    }

}

#define UP_TERMINATE(message, ...) \
    do { \
        using namespace up::literals; \
        ::up_terminate::terminate(up::source_location(), message, __VA_ARGS__); \
    } while (false)
