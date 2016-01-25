#pragma once

/*
 * The libraries of both Clang-3.8 and GCC-5 already support the (proposed)
 * string_view types. Make them available in the up namespace, so that they
 * can be used with shorter names.
 */

#include "up_include.hpp"

#include <experimental/string_view>

namespace up_string_view
{

    using std::experimental::basic_string_view;
    using std::experimental::string_view;

}

namespace up
{

    using up_string_view::basic_string_view;
    using up_string_view::string_view;

}
