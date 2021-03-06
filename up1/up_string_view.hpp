#pragma once

/*
 * The libraries of both Clang-3.8 and GCC-5 already support the (proposed)
 * string_view types. Make them available in the up namespace, so that they
 * can be used with shorter names.
 */

#include <experimental/string_view>

namespace up_string_view
{

    using std::experimental::basic_string_view;
    using std::experimental::string_view;

    /**
     * This function template is intended to be used with string-like types,
     * that do not directly support string_view themselves (in particular
     * std::string). However, there is a proposal for C++17 to add conversions
     * from and to string_view to std::string, and that will most likely
     * remove the need for this helper function.
     */
    template <typename..., typename Type>
    auto to_string_view(Type&& value) -> string_view
    {
        return {value.data(), value.size()};
    }

}

namespace up
{

    using up_string_view::basic_string_view;
    using up_string_view::string_view;
    using up_string_view::to_string_view;

}
