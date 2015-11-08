#pragma once

#include "up_include.hpp"

namespace up_impl_ptr
{

    template <typename Impl, typename Enclosing>
    class deleter final
    {
    public: // --- operations ---
        void operator()(Impl* ptr) const
        {
            Enclosing::destroy(ptr);
        }
    };

    template <typename Impl>
    class deleter<Impl, void> final
    {
    public: // --- operations ---
        void operator()(Impl* ptr) const
        {
            destroy(ptr);
        }
    };

    template <typename Impl, typename Enclosing = void>
    using impl_ptr = std::unique_ptr<Impl, deleter<Impl, Enclosing>>;

    template <typename Impl, typename Enclosing = void, typename Base = Impl, typename... Args>
    auto make_impl(Args&&... args) -> impl_ptr<Base, Enclosing>
    {
        return impl_ptr<Base, Enclosing>(new Impl(std::forward<Args>(args)...));
    }

    template <typename Impl, typename Enclosing = void>
    auto null_impl() -> impl_ptr<Impl, Enclosing>
    {
        return impl_ptr<Impl, Enclosing>(nullptr);
    }

}

namespace up
{

    using up_impl_ptr::impl_ptr;
    using up_impl_ptr::make_impl;
    using up_impl_ptr::null_impl;

}
