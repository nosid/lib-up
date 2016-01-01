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

    class message_with_implicit_source_location final
    {
    private: // --- state ---
        up::string_literal _message;
        up::source_location _location;
    public: // --- life ---
        message_with_implicit_source_location(
            up::string_literal message, up::source_location location = {})
            : _message(message), _location(location)
        { }
    public: // --- operations ---
        auto message() const { return _message; }
        auto location() const { return _location; }
    };

    [[noreturn]]
    void terminate_aux(
        up::source_location location, up::string_literal message, const up::fabrics& details);

    template <typename... Args>
    [[noreturn]]
    void terminate(message_with_implicit_source_location message, Args&&... args)
    {
        terminate_aux(message.location(), message.message(),
            up::fabrics{up::invoke_to_fabric_with_fallback(std::forward<Args>(args))...});
    }

}

namespace up
{

    using up_terminate::terminate;

}
