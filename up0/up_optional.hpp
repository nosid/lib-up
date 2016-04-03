#pragma once

/*
 * The libraries of both Clang-3.6 and GCC-4.9 already support the (proposed)
 * optional types. Make them available in the up namespace, so that they can
 * be used with shorter names.
 */

#include <experimental/optional>

#include "up_insight.hpp"

namespace up_optional
{

    using std::experimental::bad_optional_access;
    using std::experimental::optional;
    using std::experimental::in_place_t;
    using std::experimental::in_place;
    using std::experimental::nullopt_t;
    using std::experimental::nullopt;
    using std::experimental::make_optional;

}

namespace std
{

    namespace experimental
    {

        /**
         * up::insight relies on ADL. For this reason, the following function
         * is added to the std namespace, although that is not allowed
         * (according to the C++ standard).
         */
        template <typename... Types>
        auto to_insight(const optional<Types...>& arg)
        {
            return arg
                ? up::insight(typeid(arg), "exists", up::invoke_to_insight_with_fallback(*arg))
                : up::insight(typeid(arg), "nullopt");
        }

        // avoid accidental (implicit) conversion from an actual type to optional.
        template <typename... Types>
        auto to_insight(const optional<Types...>&& arg) = delete;

    }

}

namespace up
{

    using up_optional::bad_optional_access;
    using up_optional::optional;
    using up_optional::in_place_t;
    using up_optional::in_place;
    using up_optional::nullopt_t;
    using up_optional::nullopt;
    using up_optional::make_optional;

}
