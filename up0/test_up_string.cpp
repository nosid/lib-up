#include "up_string.hpp"
#include "up_test.hpp"

namespace
{

    UP_TEST_CASE {
        UP_TEST_EQUAL("foo", up::unique_string("foo"));
        UP_TEST_EQUAL(up::unique_string("foo"), "foo");
        UP_TEST_EQUAL(up::unique_string("foo"), up::unique_string("foo"));

        UP_TEST_EQUAL("foo" + up::unique_string("bar"), "foobar");
        UP_TEST_EQUAL(up::unique_string("foo") + "bar", "foobar");
        UP_TEST_EQUAL(up::unique_string("foo") + up::unique_string("bar"), "foobar");

        UP_TEST_EQUAL('b' + up::unique_string("ar"), "bar");
        UP_TEST_EQUAL(up::unique_string("ba") + 'r', "bar");

        up::shared_string a;
        up::unique_string b(a);
        up::shared_string c(b);
        b = a;
    };

}
