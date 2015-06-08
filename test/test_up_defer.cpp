#include "up_defer.hpp"
#include "up_test.hpp"

namespace
{

    UP_TEST_CASE {
        int value = 1;
        UP_DEFER { value = 2; };
        UP_TEST_EQUAL(value, 1); // update happens afterwards
    };

    UP_TEST_CASE {
        int value = 1;
        {
            UP_DEFER { value = 2; };
            UP_TEST_EQUAL(value, 1); // update happens afterwards
        }
        UP_TEST_EQUAL(value, 2);
    };

}
