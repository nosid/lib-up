#include "up_istring.hpp"
#include "up_test.hpp"

namespace
{

    UP_TEST_CASE {
        UP_TEST_EQUAL(up::istring("foo"), up::istring("foo"));
    };

}
