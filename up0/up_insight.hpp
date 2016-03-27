#pragma once

#include "up_string.hpp"
#include "up_to_string.hpp"


namespace up_adl_to_insight
{

    template <typename... Args>
    auto invoke(Args&&... args)
        -> decltype(to_insight(std::declval<Args>()...))
    {
        return to_insight(std::forward<Args>(args)...);
    }

}

namespace up_insight
{

    class insight;
    using insights = std::vector<insight>;

    class insight final
    {
    private: // --- state ---
        const std::type_info& _type_info;
        up::shared_string _value;
        insights _nested;
    public: // --- life ---
        template <typename Value, typename... Insights>
        explicit insight(const std::type_info& type_info, Value&& value, Insights&&... nested)
            : _type_info(type_info)
            , _value(std::forward<Value>(value))
            , _nested{std::forward<Insights>(nested)...}
        { }
    public: // --- operations ---
        auto type_info() const -> auto& { return _type_info; }
        auto value() const -> auto& { return _value; }
        auto nested() const -> auto& { return _nested; }
        auto to_insight() const & -> auto& { return *this; }
        auto to_insight() && -> insight&& { return std::move(*this); }
        void out(std::ostream& os) const;
    };


    template <typename Type>
    using member_to_insight_t = decltype(std::declval<Type>().to_insight());

    template <typename Type>
    using has_member_to_insight = up::is_detected<member_to_insight_t, Type>;

    template <typename Type>
    using free_to_insight_t = decltype(up_adl_to_insight::invoke(std::declval<Type>()));

    template <typename Type>
    using has_free_to_insight = up::is_detected<free_to_insight_t, Type>;


    template <typename Type>
    auto invoke_to_insight(Type&& value)
        -> std::enable_if_t<
            has_member_to_insight<Type>() || !has_free_to_insight<Type>(),
            decltype(std::declval<Type>().to_insight())>
    {
        return std::forward<Type>(value).to_insight();
    }

    template <typename Type>
    auto invoke_to_insight(Type&& value)
        -> std::enable_if_t<
            !has_member_to_insight<Type>() && has_free_to_insight<Type>(),
            decltype(up_adl_to_insight::invoke(std::declval<Type>()))>
    {
        return up_adl_to_insight::invoke(std::forward<Type>(value));
    }


    template <typename Type>
    using invoke_to_insight_t = decltype(invoke_to_insight(std::declval<Type>()));

    template <typename Type>
    using use_invoke_to_insight = up::is_detected<invoke_to_insight_t, Type>;

    template <typename Type>
    using invoke_to_string_t = decltype(up::invoke_to_string(std::declval<Type>()));

    template <typename Type>
    using use_invoke_to_string = up::is_detected<invoke_to_string_t, Type>;


    template <typename Type>
    auto invoke_to_insight_with_fallback(Type&& value)
        -> std::enable_if_t<
            use_invoke_to_insight<Type>() || !use_invoke_to_string<Type>(),
            decltype(invoke_to_insight(std::declval<Type>()))>
    {
        return invoke_to_insight(std::forward<Type>(value));
    }

    template <typename Type>
    auto invoke_to_insight_with_fallback(Type&& value)
        -> std::enable_if_t<
            !use_invoke_to_insight<Type>() && use_invoke_to_string<Type>(),
            insight>
    {
        return insight(typeid(value), up::invoke_to_string(std::forward<Type>(value)));
    }

}

namespace up
{

    using up_insight::insight;
    using up_insight::insights;
    using up_insight::invoke_to_insight;
    using up_insight::invoke_to_insight_with_fallback;

}
