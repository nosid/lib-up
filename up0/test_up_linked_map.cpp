#include "up_linked_map.hpp"
#include "up_test.hpp"

namespace
{

    UP_TEST_CASE {
        up::linked_map<up::shared_string, int> map;
        map.emplace("foo", 1);
        map.emplace("bar", 2);
    };

}
