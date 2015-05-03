#include "up_vlq.hpp"

#include "up_exception.hpp"


namespace
{

    struct overflow_error;
    struct incomplete_error;

}


[[noreturn]]
void up_vlq::raise_vlq_overflow_error(std::size_t offset, uintmax_t value, uintmax_t limit)
{
    UP_RAISE(overflow_error, "vlq-overflow-error"_s, offset, value, limit);
}

[[noreturn]]
void up_vlq::raise_vlq_incomplete_error(std::size_t offset, uintmax_t value)
{
    UP_RAISE(incomplete_error, "vlq-incomplete-error"_s, offset, value);
}
