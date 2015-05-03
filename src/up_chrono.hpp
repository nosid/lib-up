#pragma once

/**
 * This file provides aliases for types from std::chrono. There are some
 * implementation-specific parts with these types, which make them unnecessary
 * complicate to use. This file makes explicit what we expect from these
 * types, which should simplify their usage.
 */

#include <chrono>

#include "up_include.hpp"

namespace up_chrono
{

    /**
     * Unfortunately, system_time_point and steady_time_point might use
     * different resolutions. We make sure, that both use the same, and
     * provide an alias, because the duration type is frequently used.
     *
     * Unfortunately, libc++ uses a different resolution for steady_clock. To
     * have the same resolution across all compilers, the clocks are wrapped
     * into own types.
     */

    using nanoseconds = std::chrono::nanoseconds;
    using duration = nanoseconds;

    auto to_string(const duration& value) -> std::string;


    class system_clock final
    {
    private: // --- scope ---
        using self = system_clock;
        using delegate = std::chrono::system_clock;
    public:
        using duration = nanoseconds;
        using rep = duration::rep;
        using period = duration::period;
        using time_point = std::chrono::time_point<self>;
        static constexpr bool is_steady = delegate::is_steady;
        static auto now() noexcept -> time_point
        {
            return time_point(delegate::now().time_since_epoch());
        }
        static auto to_time_t(const time_point& t) noexcept -> time_t
        {
            auto other = std::chrono::duration_cast<delegate::duration>(t.time_since_epoch());
            return delegate::to_time_t(delegate::time_point(other));
        }
        static auto from_time_t(time_t t) noexcept -> time_point
        {
            return time_point(delegate::from_time_t(t).time_since_epoch());
        }
    };

    using system_time_point = system_clock::time_point;

    auto to_string(const system_time_point& value) -> std::string;


    class steady_clock final
    {
    private: // --- scope ---
        using self = steady_clock;
        using delegate = std::chrono::steady_clock;
    public:
        using duration = nanoseconds;
        using rep = duration::rep;
        using period = duration::period;
        using time_point = std::chrono::time_point<self>;
        static constexpr bool is_steady = delegate::is_steady;
        static auto now() noexcept -> time_point
        {
            return time_point(delegate::now().time_since_epoch());
        }
    };

    using steady_time_point = steady_clock::time_point;

    auto to_string(const steady_time_point& value) -> std::string;

}

namespace up
{

    using up_chrono::duration;
    using up_chrono::system_clock;
    using up_chrono::system_time_point;
    using up_chrono::steady_clock;
    using up_chrono::steady_time_point;

}

namespace std
{

    template <typename Rep, typename Period>
    auto to_string(const std::chrono::duration<Rep, Period>& value)
    {
        return up_chrono::to_string(value);
    }

    template <typename Clock, typename Duration>
    auto to_string(const std::chrono::time_point<Clock, Duration>& value)
    {
        return up_chrono::to_string(value);
    }

}
