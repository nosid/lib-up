#pragma once

#include "up_chunk.hpp"
#include "up_swap.hpp"

namespace up_buffer
{

    /**
     * This class is intended to be used to incrementally fill a buffer of
     * chars, e.g. as required for reading from a socket. It is not intended
     * to be used to transfer the data between parts of an application,
     * because there is no way to control the overhead.
     *
     * The data is split into two ranges: the warm range is followed by the
     * cold range. The warm range contains the already produces data whereas
     * the cold range is used for producing more data. The member function
     * 'produce' is used to change the split point. The member function
     * 'consume' is used to drain data from the warm range.
     */
    class buffer final
    {
    public: // --- scope ---
        using self = buffer;
        class impl;
        using size_type = std::size_t;
    private: // --- state ---
        /**
         * All information is stored behind a single pointer to keep the
         * static size of the object small.
         */
        char* _core = nullptr;
    public: // --- life ---
        explicit buffer() noexcept = default;
        buffer(const char* data, size_type size);
        buffer(up::chunk::from chunk);
        buffer(const self& rhs);
        buffer(self&& rhs) noexcept;
        ~buffer() noexcept;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self&;
        auto operator=(self&& rhs) & noexcept -> self&;
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_core, rhs._core);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }

        /**
         * Warm Range: the already produced (available) and not yet consumed
         * data.
         */

        // pointer to beginning of warm range
        auto warm() const noexcept -> const char*;
        auto warm() noexcept -> char*;
        // size of warm range
        auto available() const noexcept -> size_type;
        // drain data from warm range
        void consume(size_type n);
        // implicit conversion for typical usage of warm range
        operator up::chunk::from() const;

        /**
         * Cold Range: uninitialized memory for producing more data at the end
         * of the warm range.
         */

        // pointer to beginning of cold range (end of warm range)
        auto cold() noexcept -> char*;
        // size of cold range
        auto capacity() const noexcept -> size_type;
        // increase the size of the cold range (if necessary)
        auto reserve(size_type required_cold_size) -> self&;
        // move the point between warm and cold range
        void produce(size_type n);
        // implicit conversion for typical usage of cold range
        operator up::chunk::into();
    };

}

namespace up
{

    using up_buffer::buffer;

}
