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


    class invoke_to_fabric_aux final
    {
    private:
        template <typename Head, typename... Tail>
        static auto test_member(Head&&, Tail&&...) noexcept
            -> decltype(std::declval<Head>().to_fabric(std::declval<Tail>()...));
        static void test_member(...);
        template <typename... Args>
        static auto test_free(Args&&...) noexcept
            -> decltype(up_adl_to_fabric::invoke(std::declval<Args>()...));
        static void test_free(...);
    public:
        template <typename... Args>
        static constexpr auto member() -> bool
        {
            return noexcept(test_member(std::declval<Args>()...))
                || !noexcept(test_free(std::declval<Args>()...));
        }
    };


    template <typename Head, typename... Tail>
    auto invoke_to_fabric(Head&& head, Tail&&... tail)
        -> std::enable_if_t<
            invoke_to_fabric_aux::member<Head, Tail...>(),
            decltype(std::declval<Head>().to_fabric(std::declval<Tail>()...))>
    {
        return std::forward<Head>(head).to_fabric(std::forward<Tail>(tail)...);
    }

    template <typename... Args>
    auto invoke_to_fabric(Args&&... args)
        -> std::enable_if_t<
            !invoke_to_fabric_aux::member<Args...>(),
            decltype(up_adl_to_fabric::invoke(std::declval<Args>()...))>
    {
        return up_adl_to_fabric::invoke(std::forward<Args>(args)...);
    }


    class invoke_to_fabric_with_fallback_aux final
    {
    private:
        template <typename... Args>
        static auto test_to_fabric(Args&&...) noexcept
            -> decltype(invoke_to_fabric(std::declval<Args>()...));
        static void test_to_fabric(...);
        template <typename... Args>
        static auto test_to_string(Args&&...) noexcept
            -> decltype(up::invoke_to_string(std::declval<Args>()...));
        static void test_to_string(...);
    public:
        template <typename... Args>
        static constexpr auto use_to_fabric() -> bool
        {
            return noexcept(test_to_fabric(std::declval<Args>()...));
        }
        template <typename... Args>
        static constexpr auto use_to_string() -> bool
        {
            return !use_to_fabric<Args...>()
                && noexcept(test_to_string(std::declval<Args>()...));
        }
    };


    template <typename... Args>
    auto invoke_to_fabric_with_fallback(Args&&... args)
        -> std::enable_if_t<
            invoke_to_fabric_with_fallback_aux::use_to_fabric<Args...>(),
            decltype(invoke_to_fabric(std::declval<Args>()...))>
    {
        return invoke_to_fabric(std::forward<Args>(args)...);
    }

    template <typename Head, typename... Tail>
    auto invoke_to_fabric_with_fallback(Head&& head, Tail&&... tail)
        -> std::enable_if_t<
            invoke_to_fabric_with_fallback_aux::use_to_string<Head, Tail...>(),
            fabric>
    {
        return fabric(
            typeid(head),
            up::invoke_to_string(std::forward<Head>(head), std::forward<Tail>(tail)...));
    }

}

namespace up
{

    using up_fabric::fabric;
    using up_fabric::fabrics;
    using up_fabric::invoke_to_fabric;
    using up_fabric::invoke_to_fabric_with_fallback;

}
