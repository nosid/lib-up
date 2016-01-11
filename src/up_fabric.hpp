#pragma once

#include "up_istring.hpp"
#include "up_to_string.hpp"


namespace up_adl_to_fabric
{

    template <typename... Args>
    auto invoke(Args&&... args)
        -> decltype(to_fabric(std::declval<Args>()...))
    {
        return to_fabric(std::forward<Args>(args)...);
    }

}

namespace up_fabric
{

    class fabric;
    using fabrics = std::vector<fabric>;

    class fabric final
    {
    private: // --- state ---
        const std::type_info& _type_info;
        up::istring _value;
        fabrics _details;
    public: // --- life ---
        template <typename Value, typename... Fabrics>
        explicit fabric(const std::type_info& type_info, Value&& value, Fabrics&&... details)
            : _type_info(type_info)
            , _value(std::forward<Value>(value))
            , _details{std::forward<Fabrics>(details)...}
        { }
    public: // --- operations ---
        auto type_info() const -> auto& { return _type_info; }
        auto value() const -> auto& { return _value; }
        auto details() const -> auto& { return _details; }
        auto to_fabric() const & -> auto& { return *this; }
        auto to_fabric() && -> fabric&& { return std::move(*this); }
        void out(std::ostream& os) const;
    };


    template <typename Type>
    using member_to_fabric_t = decltype(std::declval<Type>().to_fabric());

    template <typename Type>
    using has_member_to_fabric = up::is_detected<member_to_fabric_t, Type>;

    template <typename Type>
    using free_to_fabric_t = decltype(up_adl_to_fabric::invoke(std::declval<Type>()));

    template <typename Type>
    using has_free_to_fabric = up::is_detected<free_to_fabric_t, Type>;


    template <typename Type>
    auto invoke_to_fabric(Type&& value)
        -> std::enable_if_t<
            has_member_to_fabric<Type>() || !has_free_to_fabric<Type>(),
            decltype(std::declval<Type>().to_fabric())>
    {
        return std::forward<Type>(value).to_fabric();
    }

    template <typename Type>
    auto invoke_to_fabric(Type&& value)
        -> std::enable_if_t<
            !has_member_to_fabric<Type>() && has_free_to_fabric<Type>(),
            decltype(up_adl_to_fabric::invoke(std::declval<Type>()))>
    {
        return up_adl_to_fabric::invoke(std::forward<Type>(value));
    }


    template <typename Type>
    using invoke_to_fabric_t = decltype(invoke_to_fabric(std::declval<Type>()));

    template <typename Type>
    using use_invoke_to_fabric = up::is_detected<invoke_to_fabric_t, Type>;

    template <typename Type>
    using invoke_to_string_t = decltype(up::invoke_to_string(std::declval<Type>()));

    template <typename Type>
    using use_invoke_to_string = up::is_detected<invoke_to_string_t, Type>;


    template <typename Type>
    auto invoke_to_fabric_with_fallback(Type&& value)
        -> std::enable_if_t<
            use_invoke_to_fabric<Type>() || !use_invoke_to_string<Type>(),
            decltype(invoke_to_fabric(std::declval<Type>()))>
    {
        return invoke_to_fabric(std::forward<Type>(value));
    }

    template <typename Type>
    auto invoke_to_fabric_with_fallback(Type&& value)
        -> std::enable_if_t<
            !use_invoke_to_fabric<Type>() && use_invoke_to_string<Type>(),
            fabric>
    {
        return fabric(typeid(value), up::invoke_to_string(std::forward<Type>(value)));
    }

}

namespace up
{

    using up_fabric::fabric;
    using up_fabric::fabrics;
    using up_fabric::invoke_to_fabric;
    using up_fabric::invoke_to_fabric_with_fallback;

}
