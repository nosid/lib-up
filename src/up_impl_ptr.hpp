#pragma once

#include "up_include.hpp"

namespace up_impl_ptr
{

    template <typename Type>
    using impl_ptr = std::unique_ptr<Type, void(*)(void*)>;

    template <typename Type, typename... Args>
    auto make_impl(Args&&... args) -> impl_ptr<Type>
    {
        return impl_ptr<Type>(
            new Type(std::forward<Args>(args)...),
            [](void* ptr) { std::default_delete<Type>()(static_cast<Type*>(ptr)); });
    }

    template <typename Type>
    auto null_impl() -> impl_ptr<Type>
    {
        return impl_ptr<Type>(nullptr, [](void* ptr __attribute__((unused))) { });
    }

}

namespace up
{

    using up_impl_ptr::impl_ptr;
    using up_impl_ptr::make_impl;
    using up_impl_ptr::null_impl;

}
