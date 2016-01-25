#include "up_string.hpp"
#include "up_test.hpp"

namespace
{

    UP_TEST_CASE {
        UP_TEST_EQUAL("foo", up::string("foo"));
        UP_TEST_EQUAL(up::string("foo"), "foo");
        UP_TEST_EQUAL(up::string("foo"), up::string("foo"));

        UP_TEST_EQUAL("foo" + up::string("bar"), "foobar");
        UP_TEST_EQUAL(up::string("foo") + "bar", "foobar");
        UP_TEST_EQUAL(up::string("foo") + up::string("bar"), "foobar");

        UP_TEST_EQUAL('b' + up::string("ar"), "bar");
        UP_TEST_EQUAL(up::string("ba") + 'r', "bar");
    };

}
