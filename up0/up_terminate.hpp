#pragma once

/*
 * This file provides a macro to terminate the program in case of fatal
 * errors, that can not be handled by throwing an exception. Most importantly
 * if a thrown exception might (to easily) cause undefined behavior.
 */


#include "up_insight.hpp"
#include "up_source.hpp"

namespace up_terminate
{

    [[noreturn]]
    void terminate_aux(const up::source& source, const up::insights& insights);

    template <typename... Args>
    [[noreturn]]
    void terminate(const up::source& source, Args&&... args)
    {
        terminate_aux(source,
            up::insights{up::invoke_to_insight_with_fallback(std::forward<Args>(args))...});
    }

}

namespace up
{

    using up_terminate::terminate;

}
