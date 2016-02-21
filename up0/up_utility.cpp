#include "up_utility.hpp"

#include <cstdarg>

#include <cxxabi.h> // GCC specific header

#include "up_defer.hpp"
#include "up_exception.hpp"
#include "up_ints.hpp"


auto up_utility::type_display_name(const std::type_info& type_info) -> std::string
{
    int status = 0;
    char* result = abi::__cxa_demangle(type_info.name(), nullptr, nullptr, &status);
    UP_DEFER { std::free(result); };
    if (status == 0) {
        return std::string(result);
    } else {
        throw up::make_exception("type-demangle-error").with(type_info.name(), status);
    }
}


__attribute__((__format__(__printf__, 2, 0)))
void up_utility::cformat(up::buffer& buffer, const char* format, va_list ap)
{
    va_list aq;
    va_copy(aq, ap);
    int rv = std::vsnprintf(buffer.cold(), buffer.capacity(), format, aq);
    va_end(aq);
    if (rv < 0) {
        throw up::make_exception("cformat-error").with(std::string(format), rv);
    }
    auto size = up::ints::cast<std::size_t>(rv);
    if (size < buffer.capacity()) {
        buffer.produce(size);
    } else {
        using sizes = up::ints::domain<up::buffer::size_type>::or_range_error;
        buffer.reserve(sizes::add(size, 1));
        rv = std::vsnprintf(buffer.cold(), buffer.capacity(), format, ap);
        if (rv < 0) {
            throw up::make_exception("cformat-error").with(std::string(format), size, rv);
        }
        size = up::ints::cast<std::size_t>(rv);
        if (size < buffer.capacity()) {
            buffer.produce(size);
        } else {
            throw up::make_exception("cformat-error").with(std::string(format), size);
        }
    }
}


__attribute__((__format__(__printf__, 1, 0)))
auto up_utility::cformat(const char* format, va_list ap) -> std::string
{
    auto buffer = up::buffer();
    cformat(buffer, format, ap);
    return std::string(buffer.warm(), buffer.available());
}


void up_utility::raise_enum_set_runtime_error(up::source source, up::insight insight)
{
    throw up::make_exception(std::move(source)).with(std::move(insight));
}


thread_local up_utility::context_frame_base* up_utility::context_frame_base::top = nullptr;

void up_utility::context_frame_base::walk(const visitor& visitor)
{
    for (context_frame_base* current = top; current; current = current->_parent) {
        current->_walk(visitor);
    }
}

void up_utility::context_frame_base::_vtable_dummy() const { }


void up_utility::context_frame_walk(const context_frame_base::visitor& visitor)
{
    context_frame_base::walk(visitor);
}
