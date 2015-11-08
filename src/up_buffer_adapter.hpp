#pragma once

#include "up_buffer.hpp"
#include "up_impl_ptr.hpp"

namespace up_buffer_adapter
{

    class buffer_adapter final
    {
    public: // --- scope ---
        class reader;
        class consumer;
        class producer;
    };


    class buffer_adapter::reader final
    {
    public: // --- scope ---
        using self = reader;
        class impl;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, self> _impl;
    public: // --- life ---
        explicit reader(const up::buffer& buffer);
        // avoid temporary objects (risk of dangling references)
        explicit reader(const up::buffer&& buffer) = delete;
        reader(const self& rhs) = delete;
        reader(self&& rhs) noexcept = delete;
        ~reader() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
        operator FILE*();
    };


    class buffer_adapter::consumer final
    {
    public: // --- scope ---
        using self = consumer;
        class impl;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, self> _impl;
    public: // --- life ---
        explicit consumer(up::buffer& buffer);
        consumer(const self& rhs) = delete;
        consumer(self&& rhs) noexcept = delete;
        ~consumer() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
        operator FILE*();
    };


    class buffer_adapter::producer final
    {
    public: // --- scope ---
        using self = producer;
        class impl;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, self> _impl;
    public: // --- life ---
        explicit producer(up::buffer& buffer);
        producer(const self& rhs) = delete;
        producer(self&& rhs) noexcept = delete;
        ~producer() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
        operator FILE*();
    };

}

namespace up
{

    using up_buffer_adapter::buffer_adapter;

}
