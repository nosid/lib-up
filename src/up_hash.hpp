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
     *
     * Security: The hash function is crypto-graphically weak, and an attacker
     * might generate strings to intentionally cause collisions. This function
     * should not be used, if the strings originate from an untrusted source.
     */

    auto fnv1a(const unsigned char* data, std::size_t size) noexcept -> std::size_t;
    auto fnv1a(const char* data, std::size_t size) noexcept -> std::size_t;
    auto fnv1a(up::chunk::from chunk) noexcept -> std::size_t;

}

namespace up
{

    using up_hash::fnv1a;

}
