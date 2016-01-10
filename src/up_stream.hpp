#pragma once

#include "up_chrono.hpp"
#include "up_chunk.hpp"
#include "up_impl_ptr.hpp"
#include "up_swap.hpp"
#include "up_utility.hpp"


namespace up_stream
{

    class stream
    {
    public: // --- scope ---
        using self = stream;
        enum class native_handle : int { invalid = -1, };
        class timeout;
        class await;
        class steady_await;
        class deadline_await;
        class infinite_await;
        class engine;
    private: // --- state ---
        std::unique_ptr<engine> _engine;
    public: // --- life ---
        explicit stream(std::unique_ptr<engine> engine);
        stream(const self& rhs) = delete;
        stream(self&& rhs) noexcept = default;
        virtual ~stream() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_engine, rhs._engine);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        void shutdown(await& awaiting) const;
        void shutdown(await&& awaiting) const
        {
            shutdown(awaiting);
        }
        void graceful_close(await& awaiting) const;
        void graceful_close(await&& awaiting) const
        {
            graceful_close(awaiting);
        }
        auto read_some(up::chunk::into chunk, await& awaiting) const -> std::size_t;
        auto read_some(up::chunk::into chunk, await&& awaiting) const -> std::size_t
        {
            return read_some(std::move(chunk), awaiting);
        }
        auto write_some(up::chunk::from chunk, await& awaiting) const -> std::size_t;
        auto write_some(up::chunk::from chunk, await&& awaiting) const -> std::size_t
        {
            return write_some(std::move(chunk), awaiting);
        }
        auto read_some(up::chunk::into_bulk_t&& chunks, await& awaiting) const -> std::size_t;
        auto read_some(up::chunk::into_bulk_t&& chunks, await&& awaiting) const -> std::size_t
        {
            return read_some(std::move(chunks), awaiting);
        }
        auto write_some(up::chunk::from_bulk_t&& chunks, await& awaiting) const -> std::size_t;
        auto write_some(up::chunk::from_bulk_t&& chunks, await&& awaiting) const -> std::size_t
        {
            return write_some(std::move(chunks), awaiting);
        }
        void write_all(up::chunk::from chunk, await& awaiting) const;
        void write_all(up::chunk::from chunk, await&& awaiting) const
        {
            write_all(std::move(chunk), awaiting);
        }
        void write_all(up::chunk::from_bulk_t&& chunks, await& awaiting) const;
        void write_all(up::chunk::from_bulk_t&& chunks, await&& awaiting) const
        {
            write_all(std::move(chunks), awaiting);
        }
        void upgrade(std::function<std::unique_ptr<engine>(std::unique_ptr<engine>)> transform);
        void downgrade(await& awaiting);
        void downgrade(await&& awaiting)
        {
            downgrade(awaiting);
        }
    protected:
        auto get_underlying_engine() const -> const engine*;
    private:
        // classes with vtables should have at least one out-of-line virtual method definition
        __attribute__((unused))
        virtual void _vtable_dummy() const;
    };


    class stream::await
    {
    public: // --- scope ---
        enum class operation { read, write, };
    protected: // --- life ---
        ~await() noexcept = default;
    public: // --- operations ---
        void operator()(native_handle handle, operation op)
        {
            _wait(handle, op);
        }
    private:
        virtual void _wait(native_handle handle, operation op) = 0;
        // classes with vtables should have at least one out-of-line virtual method definition
        __attribute__((unused))
        virtual void _vtable_dummy() const;
    };

    auto to_string(stream::await::operation op) -> std::string;


    class stream::steady_await final : public stream::await
    {
    public : // --- scope ---
        using self = steady_await;
    private: // --- state ---
        up::steady_time_point& _now;
        up::steady_time_point _deadline;
        up::duration _duration;
    public: // --- life ---
        /* The argument now is passed as lvalue reference, and will be
         * automatically updated in the wait method. */
        explicit steady_await(up::steady_time_point& now, const up::steady_time_point& deadline)
            : _now(now), _deadline(deadline), _duration(deadline - now)
        { }
        template <typename Rep, typename Period>
        explicit steady_await(up::steady_time_point& now, const std::chrono::duration<Rep, Period>& duration)
            : self(now, now + duration)
        { }
    private: // --- operations ---
        virtual void _wait(native_handle handle, operation op) override;
    };


    class stream::deadline_await final : public stream::await
    {
    public : // --- scope ---
        using self = deadline_await;
        class impl;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, destroy> _impl;
    public: // --- life ---
        explicit deadline_await();
        explicit deadline_await(const up::system_time_point& expires_at);
        explicit deadline_await(const up::steady_time_point& expires_at);
        explicit deadline_await(const up::duration& expires_from_now);
        deadline_await(const self& rhs) = delete;
        deadline_await(self&& rhs) noexcept = default;
        ~deadline_await() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        auto operator=(const up::system_time_point& expires_at) & -> self&;
        auto operator=(const up::steady_time_point& expires_at) & -> self&;
        auto operator=(const up::duration& expires_from_now) & -> self&;
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
    private:
        virtual void _wait(native_handle handle, operation op) override;
    };


    class stream::infinite_await final : public stream::await
    {
    public : // --- scope ---
        using self = infinite_await;
    public: // --- life ---
        explicit infinite_await() = default;
        infinite_await(const self& rhs) = delete;
        infinite_await(self&& rhs) noexcept = default;
        ~infinite_await() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        void swap(self& rhs __attribute__((unused))) noexcept { }
        friend void swap(self& lhs __attribute__((unused)), self& rhs __attribute__((unused))) noexcept { }
    private:
        virtual void _wait(native_handle handle, operation op) override;
    };


    class stream::engine
    {
    public: // --- scope ---
        using self = engine;
        struct unreadable { };
        struct unwritable { };
    public: // --- life ---
        virtual ~engine() noexcept = default;
    protected:
        engine() = default;
        engine(const self& rhs) = default;
        engine(self&& rhs) noexcept = default;
    public: // --- operations ---
        virtual void shutdown() const = 0;
        virtual void hard_close() const = 0; // hard close, not graceful
        virtual auto read_some(up::chunk::into chunk) const -> std::size_t = 0;
        virtual auto write_some(up::chunk::from chunk) const -> std::size_t = 0;
        virtual auto read_some_bulk(up::chunk::into_bulk_t& chunks) const -> std::size_t = 0;
        virtual auto write_some_bulk(up::chunk::from_bulk_t& chunks) const -> std::size_t = 0;
        virtual auto downgrade() -> std::unique_ptr<engine> = 0;
        virtual auto get_underlying_engine() const -> const engine* = 0;
        virtual auto get_native_handle() const -> native_handle = 0;
    protected:
        auto operator=(const self& rhs) & -> self& = default;
        auto operator=(self&& rhs) & noexcept -> self& = default;
    private:
        // classes with vtables should have at least one out-of-line virtual method definition
        __attribute__((unused))
        virtual void _vtable_dummy() const;
    };

}

namespace up
{

    using up_stream::stream;

}
