#pragma once

/*
 * This file provides functions/classes to support character conversions from
 * different character sets and different encodings. It is based on and wraps
 * libc' iconv function.
 */

#include "up_impl_ptr.hpp"
#include "up_string_view.hpp"
#include "up_swap.hpp"

namespace up_iconv
{

    /*
     * This class is (intentionally) not thread-safe. For thread-safe
     * conversions, use shared_iconv instead.
     */
    class unique_iconv final
    {
    public: // --- scope ---
        using self = unique_iconv;
        class impl;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, destroy> _impl;
    public: // --- life ---
        explicit unique_iconv(std::string to, std::string from);
        unique_iconv(const self& rhs) = delete;
        unique_iconv(self&& rhs) noexcept = default;
        ~unique_iconv() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        /*
         * Transform the given string. If the conversion fails, an exception
         * is thrown. This operator is intentionally non-const, because it is
         * not thread-safe.
         *
         * Hint: There is currently no bulk support, because non-consecutive
         * buffers are not easy to use with the underlying iconv library.
         */
        auto operator()(up::string_view string) -> std::string;
    };


    /*
     * This class is (intentionally) thread-safe. It requires some overhead
     * for synchronization. For non-thread-safe conversions, use unique_iconv
     * instead.
     */
    class shared_iconv final
    {
    public: // --- scope ---
        using self = shared_iconv;
        class impl;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, destroy> _impl;
    public: // --- life ---
        explicit shared_iconv(std::string to, std::string from);
        shared_iconv(const self& rhs) = delete;
        shared_iconv(self&& rhs) noexcept = default;
        ~shared_iconv() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        /*
         * Transform the given string. If the conversion fails, an exception
         * is thrown. This operator is intentionally const, because it is
         * thread-safe.
         *
         * Hint: There is currently no bulk support, because non-consecutive
         * buffers are not easy to use with the underlying iconv library.
         */
        auto operator()(up::string_view string) const -> std::string;
    };

}

namespace up
{

    using up_iconv::unique_iconv;
    using up_iconv::shared_iconv;

}
