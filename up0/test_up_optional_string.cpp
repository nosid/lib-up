#include "up_optional_string.hpp"
#include "up_test.hpp"

namespace
{

    UP_TEST_CASE {
        using os = up::optional_string;
        UP_TEST_FALSE(os() < os());
        UP_TEST_TRUE(os() < os(""));
        UP_TEST_TRUE(os() < os("foo"));
        UP_TEST_TRUE(os() < os("bar"));
        UP_TEST_FALSE(os("") < os());
        UP_TEST_FALSE(os("") < os(""));
        UP_TEST_TRUE(os("") < os("foo"));
        UP_TEST_TRUE(os("") < os("bar"));
        UP_TEST_FALSE(os("foo") < os());
        UP_TEST_FALSE(os("foo") < os(""));
        UP_TEST_FALSE(os("foo") < os("foo"));
        UP_TEST_FALSE(os("foo") < os("bar"));
        UP_TEST_FALSE(os("bar") < os());
        UP_TEST_FALSE(os("bar") < os(""));
        UP_TEST_TRUE(os("bar") < os("foo"));
        UP_TEST_FALSE(os("bar") < os("bar"));
    };

}
