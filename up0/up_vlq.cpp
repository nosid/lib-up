#include "up_vlq.hpp"

#include "up_exception.hpp"


[[noreturn]]
void up_vlq::raise_vlq_overflow_error(std::size_t offset, uintmax_t value, uintmax_t limit)
{
    throw up::make_exception<std::overflow_error>("vlq-overflow-error").with(offset, value, limit);
}

[[noreturn]]
void up_vlq::raise_vlq_incomplete_error(std::size_t offset, uintmax_t value)
{
    throw up::make_exception<std::underflow_error>("vlq-incomplete-error").with(offset, value);
}
