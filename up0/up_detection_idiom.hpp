#pragma once

namespace up_detection_idiom
{

    /**
     * The following four templates realize the detection idiom, that has been
     * proposed for C++17, but is neither supported by GCC-5 nor by Clang-3.8.
     */

    template <typename AlwaysVoid, template <typename...> typename Op, typename... Args>
    struct detector final
    {
        using value_t = std::false_type;
    };

    template <template <typename...> typename Op, typename... Args>
    struct detector<std::void_t<Op<Args...>>, Op, Args...> final
    {
        using value_t = std::true_type;
    };

    template <template <typename...> typename Op, typename... Args>
    using is_detected = typename detector<void, Op, Args...>::value_t;

}

namespace up
{

    using up_detection_idiom::is_detected;

}
