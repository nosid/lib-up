#include "up_stream.hpp"

#include <poll.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "up_exception.hpp"
#include "up_integral_cast.hpp"
#include "up_utility.hpp"
#include "up_terminate.hpp"


namespace
{

    using namespace up::literals;

    struct runtime { };

    void check_state(const up::impl_ptr<up::stream::engine>& engine)
    {
        if (!engine) {
            UP_RAISE(runtime, "invalid-stream-engine-state"_s);
        }
    }

    void close_aux(int& fd)
    {
        if (fd != -1) {
            int temp = std::exchange(fd, -1);
            int rv = ::close(temp);
            if (rv != 0) {
                UP_TERMINATE("bad-close"_s, temp);
            }
        } // else: nothing
    }

    auto make_poll_events(up_stream::stream::await::operation op) -> short
    {
        switch (op) {
        case up_stream::stream::await::operation::read:
            return POLLIN;
        case up_stream::stream::await::operation::write:
            return POLLOUT;
        }
        UP_RAISE(runtime, "unexpected-stream-await-operation"_s, op);
    }

    template <typename... Handles>
    auto do_poll(
        up_stream::stream::await::operation op,
        up_stream::stream::native_handle handle,
        Handles... handles)
        -> std::size_t
    {
        pollfd fds[] = {
            {up::to_underlying_type(handle), make_poll_events(op), 0},
            {handles, POLLIN, 0}...,
        };
        constexpr std::size_t nfds = 1 + sizeof...(Handles);
        for (;;) {
            int rv = ::ppoll(fds, nfds, nullptr, nullptr);
            if (rv > 0) {
                constexpr auto valid = POLLIN | POLLOUT | POLLHUP | POLLERR;
                for (std::size_t i = 0; i != nfds; ++i) {
                    if (fds[i].revents & ~valid) {
                        UP_RAISE(runtime, "invalid-stream-poll-events"_s,
                            op, fds[i].events, fds[i].revents);
                    }
                }
                for (std::size_t i = 0; i != nfds; ++i) {
                    if (fds[i].revents & valid) {
                        return i;
                    }
                }
                UP_RAISE(runtime, "unexpected-stream-poll-status"_s, op);
            } else if (rv == 0) {
                UP_RAISE(runtime, "unexpected-stream-poll-status"_s, op);
            } else if (errno == EINTR) {
                // restart
            } else {
                UP_RAISE(runtime, "stream-poll-error"_s, op, up::errno_info(errno));
            }
        }
    }

}


up_stream::stream::stream(up::impl_ptr<engine> engine)
    : _engine(std::move(engine))
{
    check_state(_engine);
}

void up_stream::stream::shutdown(await& awaiting) const
{
    check_state(_engine);
    for (;;) {
        try {
            return _engine->shutdown();
        } catch (const up::exception<engine::unreadable>&) {
            awaiting(_engine->get_native_handle(), await::operation::read);
        } catch (const up::exception<engine::unwritable>&) {
            awaiting(_engine->get_native_handle(), await::operation::write);
        }
    }
}

void up_stream::stream::graceful_close(await& awaiting) const
{
    check_state(_engine);
    shutdown(awaiting);
    for (;;) {
        try {
            char c;
            if (_engine->read_some({&c, 1})) {
                UP_RAISE(runtime, "stream-graceful-close-error"_s,
                    up::to_underlying_type(_engine->get_native_handle()));
            } else {
                break;
            }
        } catch (const up::exception<engine::unreadable>&) {
            awaiting(_engine->get_native_handle(), await::operation::read);
        } catch (const up::exception<engine::unwritable>&) {
            awaiting(_engine->get_native_handle(), await::operation::write);
        }
    }
    _engine->hard_close();
}

auto up_stream::stream::read_some(up::chunk::into chunk, await& awaiting) const -> std::size_t
{
    check_state(_engine);
    for (;;) {
        try {
            return _engine->read_some(chunk);
        } catch (const up::exception<engine::unreadable>&) {
            awaiting(_engine->get_native_handle(), await::operation::read);
        } catch (const up::exception<engine::unwritable>&) {
            awaiting(_engine->get_native_handle(), await::operation::write);
        }
    }
}

auto up_stream::stream::write_some(up::chunk::from chunk, await& awaiting) const -> std::size_t
{
    check_state(_engine);
    for (;;) {
        try {
            return _engine->write_some(chunk);
        } catch (const up::exception<engine::unreadable>&) {
            awaiting(_engine->get_native_handle(), await::operation::read);
        } catch (const up::exception<engine::unwritable>&) {
            awaiting(_engine->get_native_handle(), await::operation::write);
        }
    }
}

auto up_stream::stream::readv(up::chunk::into_bulk_t&& chunks, await& awaiting) const -> std::size_t
{
    check_state(_engine);
    for (;;) {
        try {
            return _engine->readv(chunks);
        } catch (const up::exception<engine::unreadable>&) {
            awaiting(_engine->get_native_handle(), await::operation::read);
        } catch (const up::exception<engine::unwritable>&) {
            awaiting(_engine->get_native_handle(), await::operation::write);
        }
    }
}

auto up_stream::stream::writev(up::chunk::from_bulk_t&& chunks, await& awaiting) const -> std::size_t
{
    check_state(_engine);
    for (;;) {
        try {
            return _engine->writev(chunks);
        } catch (const up::exception<engine::unreadable>&) {
            awaiting(_engine->get_native_handle(), await::operation::read);
        } catch (const up::exception<engine::unwritable>&) {
            awaiting(_engine->get_native_handle(), await::operation::write);
        }
    }
}

void up_stream::stream::upgrade(std::function<up::impl_ptr<engine>(up::impl_ptr<engine>)> transform)
{
    check_state(_engine);
    _engine = transform(std::move(_engine));
}

auto up_stream::stream::get_underlying_engine() const -> const engine*
{
    check_state(_engine);
    return _engine->get_underlying_engine();
}


auto up_stream::to_string(stream::await::operation op) -> std::string
{
    switch (op) {
    case stream::await::operation::read:
        return up::invoke_to_string("read"_s);
    case stream::await::operation::write:
        return up::invoke_to_string("write"_s);
    }
    UP_RAISE(runtime, "unexpected-stream-await-operation"_s, op);
}


void up_stream::stream::steady_await::_wait(native_handle handle, operation op)
{
    pollfd fds{up::to_underlying_type(handle), make_poll_events(op), 0};
    for (;;) {
        auto remaining = std::max(_deadline - _now, up::duration::zero());
        auto s = std::chrono::duration_cast<std::chrono::seconds>(remaining);
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(remaining - s);
        timespec ts{up::integral_caster(s.count()), up::integral_caster(ns.count())};
        int rv = ::ppoll(&fds, 1, &ts, nullptr);
        if (rv > 0) {
            auto valid = POLLIN | POLLOUT | POLLHUP | POLLERR;
            if (fds.revents & ~valid) {
                UP_RAISE(runtime, "invalid-stream-poll-events"_s, op, fds.events, fds.revents);
            } else if (fds.revents & valid) {
                return;
            } else {
                UP_RAISE(runtime, "unexpected-stream-poll-status"_s, op, fds.events, fds.revents);
            }
        } else if (rv == 0) {
            _now = up::steady_clock::now();
            UP_RAISE(timeout, "stream-steady-await-timeout"_s, op, _deadline, _duration);
        } else if (errno == EINTR) {
            // restart
            _now = up::steady_clock::now();
        } else {
            UP_RAISE(runtime, "stream-steady-await-error"_s, op, up::errno_info(errno));
        }
    }
}


class up_stream::stream::deadline_await::impl final
{
public: // --- scope ---
    using self = impl;
    static auto make(up::impl_ptr<self>&& old, int clockid, const up::duration& duration, bool absolute)
        -> up::impl_ptr<self>
    {
        if (old && old->update(clockid, duration, absolute)) {
            return std::move(old);
        } else {
            return up::make_impl<self>(clockid, duration, absolute);
        }
    }
private: // --- fields ---
    int _clockid;
    int _fd;
public: // --- life ---
    explicit impl(int clockid, const up::duration& duration, bool absolute)
        : impl(clockid)
    {
        update(clockid, duration, absolute);
    }
    impl(const self& rhs) = delete;
    impl(self&& rhs) noexcept = delete;
    ~impl() noexcept
    {
        close_aux(_fd);
    }
private:
    explicit impl(int clockid)
        : _clockid(clockid)
        , _fd(::timerfd_create(_clockid, TFD_CLOEXEC | TFD_NONBLOCK))
    {
        if (_fd == -1) {
            UP_RAISE(runtime, "deadline-timer-creation-error"_s, _clockid, up::errno_info(errno));
        }
    }
public: // --- operations ---
    auto operator=(const self& rhs) & -> self& = delete;
    auto operator=(self&& rhs) & noexcept -> self& = delete;
    bool update(int clockid, const up::duration& duration, bool absolute)
    {
        if (clockid == _clockid) {
            auto s = std::chrono::duration_cast<std::chrono::seconds>(duration);
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - s);
            itimerspec its = {{0, 0}, {up::integral_caster(s.count()), up::integral_caster(ns.count())}};
            if (its.it_value.tv_sec <= 0 && its.it_value.tv_nsec <= 0) {
                // adjust values - otherwise timer will be disarmed
                its.it_value.tv_sec = 0;
                its.it_value.tv_nsec = 1;
            }
            itimerspec old = {{0, 0}, {0, 0}};
            int rv = ::timerfd_settime(_fd, absolute ? TFD_TIMER_ABSTIME : 0, &its, &old);
            if (rv != 0) {
                UP_RAISE(runtime, "deadline-timer-set-failed"_s,
                    _clockid, duration, absolute, up::errno_info(errno));
            }
            return old.it_value.tv_sec || old.it_value.tv_nsec;
        } else {
            return false;
        }
    }
    void wait(native_handle handle, operation op) const
    {
        if (do_poll(op, handle, _fd) == 1) {
            UP_RAISE(timeout, "stream-deadline-await-timeout"_s, op, _clockid);
        }
    }
};


up_stream::stream::deadline_await::deadline_await()
    : _impl(up::null_impl<impl>())
{ }

up_stream::stream::deadline_await::deadline_await(const up::system_time_point& expires_at)
    : _impl(up::make_impl<impl>(CLOCK_REALTIME, expires_at.time_since_epoch(), true))
{ }

up_stream::stream::deadline_await::deadline_await(const up::steady_time_point& expires_at)
    : _impl(up::make_impl<impl>(CLOCK_MONOTONIC, expires_at.time_since_epoch(), true))
{ }

up_stream::stream::deadline_await::deadline_await(const up::duration& expires_from_now)
    : _impl(up::make_impl<impl>(CLOCK_MONOTONIC, expires_from_now, false))
{ }

auto up_stream::stream::deadline_await::operator=(const up::system_time_point& expires_at) & -> self&
{
    _impl = impl::make(std::move(_impl), CLOCK_REALTIME, expires_at.time_since_epoch(), true);
    return *this;
}

auto up_stream::stream::deadline_await::operator=(const up::steady_time_point& expires_at) & -> self&
{
    _impl = impl::make(std::move(_impl), CLOCK_MONOTONIC, expires_at.time_since_epoch(), true);
    return *this;
}

auto up_stream::stream::deadline_await::operator=(const up::duration& expires_from_now) & -> self&
{
    _impl = impl::make(std::move(_impl), CLOCK_MONOTONIC, expires_from_now, false);
    return *this;
}

void up_stream::stream::deadline_await::_wait(native_handle handle, operation op)
{
    if (_impl) {
        _impl->wait(handle, op);
    } else {
        do_poll(op, handle);
    }
}


void up_stream::stream::infinite_await::_wait(native_handle handle, operation op)
{
    do_poll(op, handle);
}
