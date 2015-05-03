#pragma once

#include "up_chunk.hpp"

namespace up_hash
{

    /**
     * Implementation of the Fowler–Noll–Vo hash function. See below URLs for
     * more information. This hash function is used because it is quite simple
     * to implement. However, there are also interesting alternatives like for
     * example farm hash.
     *
     * http://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
     * http://www.isthe.com/chongo/tech/comp/fnv/index.html
     */

    auto fnv1a(const unsigned char* data, std::size_t size) noexcept -> std::size_t;
    auto fnv1a(const char* data, std::size_t size) noexcept -> std::size_t;
    auto fnv1a(up::chunk::from chunk) noexcept -> std::size_t;

}

namespace up
{

    using up_hash::fnv1a;

}
