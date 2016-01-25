#pragma once

/**
 * This file contains the class template up::pack, which is similar to a
 * tuple. In contrast to std::tuple, it is designed to be light-weight
 * (regarding dependencies), because it is required from some other core
 * components.
 */

namespace up_pack
{

    template <std::size_t Index, typename Type>
    class pack_head
    {
    public: // --- state ---
        Type _value;
    public: // --- life ---
        template <typename T>
        explicit pack_head(T&& value) noexcept(std::is_nothrow_constructible<Type, T&&>{})
            : _value(std::forward<T>(value))
        { }
    };

    template <std::size_t Index, typename... Types>
    class pack_base;

    template <std::size_t Index>
    class pack_base<Index> { };

    template <std::size_t Index, typename Head, typename... Tail>
    class pack_base<Index, Head, Tail...>
        : public pack_head<Index, Head>, public pack_base<Index + 1, Tail...>
    {
    public: // --- scope ---
        using base_head = pack_head<Index, Head>;
        using base_tail = pack_base<Index + 1, Tail...>;
    public: // --- life ---
        template <typename H, typename... T>
        explicit pack_base(H&& head, T&&... tail)
            noexcept(std::is_nothrow_constructible<base_head, H&&>{}
                && std::is_nothrow_constructible<base_tail, T&&...>{})
            : base_head(std::forward<H>(head))
            , base_tail(std::forward<T>(tail)...)
        { }
    };

    template <typename... Types>
    class pack : public pack_base<0, Types...>
    {
    public: // --- scope ---
        using base = pack_base<0, Types...>;
    public: // --- life ---
        template <typename... Args>
        explicit pack(Args&&... values)
            noexcept(std::is_nothrow_constructible<base, Args&&...>{})
            : base(std::forward<Args>(values)...)
        { }
    };

    template <std::size_t Index, typename Type>
    auto pack_get(const pack_head<Index, Type>& head) noexcept -> const Type&
    {
        return head._value;
    }

    template <std::size_t Index, typename Type>
    auto pack_get(pack_head<Index, Type>& head) noexcept -> Type&
    {
        return head._value;
    }

}

namespace up
{

    using up_pack::pack;
    using up_pack::pack_get;

}
