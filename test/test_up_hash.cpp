#include "up_hash.hpp"
#include "up_test.hpp"

namespace
{

    UP_TEST_CASE {
        UP_TEST_EQUAL(up::fnv1a("test", 0), UINTMAX_C(14695981039346656037));
        UP_TEST_EQUAL(up::fnv1a("test", 1), UINTMAX_C(12638201494206808739));
        UP_TEST_EQUAL(up::fnv1a("test", 2), UINTMAX_C(632811855847011954));
        UP_TEST_EQUAL(up::fnv1a("test", 3), UINTMAX_C(6261330701100204979));
        UP_TEST_EQUAL(up::fnv1a("test", 4), UINTMAX_C(18007334074686647077));
    };

}
