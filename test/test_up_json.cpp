#include "up_json.hpp"
#include "up_test.hpp"

namespace
{

    UP_TEST_CASE {
        using jb = up::json::builder;
        up::json::value v = jb::array{
            nullptr,
            true,
            1.0,
        };
        up::json::value w = jb::object{
            {"foo", 1.0},
            {"bar", true},
        };
    };

}
