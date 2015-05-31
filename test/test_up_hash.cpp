#include "up_hash.hpp"
#include "up_test.hpp"

namespace
{

    UP_TEST_CASE {
        UP_TEST_EQUAL(up::fnv1a("test", 4), 18007334074686647077u);
    };

}
