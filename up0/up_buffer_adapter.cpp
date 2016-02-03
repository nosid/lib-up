#include "up_buffer_adapter.hpp"

#include <cstring>

#include "up_exception.hpp"
#include "up_ints.hpp"

namespace
{

    struct runtime;


    template <typename Derived>
    class adapter
    {
    public: // --- scope ---
        using self = adapter;
        static auto _cookie_read(void* cookie, char* data, std::size_t size) -> ssize_t
        {
            return static_cast<Derived*>(cookie)->read(data, size);
        }
        static auto _cookie_write(void* cookie, const char* data, std::size_t size) -> ssize_t
        {
            return static_cast<Derived*>(cookie)->write(data, size);
        }
    private: // --- fields ---
        FILE* _file = nullptr;
    public: // --- life ---
        explicit adapter(const char* mode, cookie_read_function_t read, cookie_write_function_t write)
        {
            _file = ::fopencookie(static_cast<Derived*>(this), mode,
                cookie_io_functions_t{read, write, nullptr, nullptr});
            if (_file == nullptr) {
                up::raise<runtime>("buffer-adapter-error");
            }
        }
        adapter(const self& rhs) = delete;
        adapter(self&& rhs) noexcept = delete;
    protected:
        ~adapter() noexcept
        {
            if (_file) {
                ::fclose(_file);
            }
        }
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
        auto get_file() { return _file; }
    };

}


class up_buffer_adapter::buffer_adapter::reader::impl : public adapter<impl>
{
private: // --- state ---
    const up::buffer& _buffer;
    std::size_t _offset = 0;
public: // --- life ---
    explicit impl(const up::buffer& buffer)
        : adapter("rb", &_cookie_read, nullptr), _buffer(buffer), _offset(0)
    { }
public: // --- operations ---
    auto read(char* data, std::size_t size) -> ssize_t
    {
        auto n = std::min(size, _buffer.available() - _offset);
        std::memcpy(data, _buffer.warm() + _offset, n);
        _offset += n;
        return up::ints::caster(n);
    }
};


void up_buffer_adapter::buffer_adapter::reader::destroy(impl* ptr)
{
    std::default_delete<impl>()(ptr);
}

up_buffer_adapter::buffer_adapter::reader::reader(const up::buffer& buffer)
    : _impl(up::impl_make(buffer))
{ }

up_buffer_adapter::buffer_adapter::reader::operator FILE*()
{
    return _impl->get_file();
}


class up_buffer_adapter::buffer_adapter::consumer::impl : public adapter<impl>
{
private: // --- state ---
    up::buffer& _buffer;
public: // --- life ---
    explicit impl(up::buffer& buffer)
        : adapter("rb", &_cookie_read, nullptr), _buffer(buffer)
    { }
public: // --- operations ---
    auto read(char* data, std::size_t size) -> ssize_t
    {
        auto n = std::min(size, _buffer.available());
        std::memcpy(data, _buffer.warm(), n);
        _buffer.consume(n);
        return up::ints::caster(n);
    }
};


void up_buffer_adapter::buffer_adapter::consumer::destroy(impl* ptr)
{
    std::default_delete<impl>()(ptr);
}

up_buffer_adapter::buffer_adapter::consumer::consumer(up::buffer& buffer)
    : _impl(up::impl_make(buffer))
{ }

up_buffer_adapter::buffer_adapter::consumer::operator FILE*()
{
    return _impl->get_file();
}


class up_buffer_adapter::buffer_adapter::producer::impl : public adapter<impl>
{
private: // --- state ---
    up::buffer& _buffer;
public: // --- life ---
    explicit impl(up::buffer& buffer)
        : adapter("wb", nullptr, &_cookie_write), _buffer(buffer)
    { }
public: // --- operations ---
    auto write(const char* data, std::size_t size) -> ssize_t
    {
        try {
            _buffer.reserve(size);
            std::memcpy(_buffer.cold(), data, size);
            _buffer.produce(size);
            return up::ints::caster(size);
        } catch (...) {
            up::suppress_current_exception("buffer-adapter");
            return 0; // signal error (see also man page for fopencookie)
        }
    }
};


void up_buffer_adapter::buffer_adapter::producer::destroy(impl* ptr)
{
    std::default_delete<impl>()(ptr);
}

up_buffer_adapter::buffer_adapter::producer::producer(up::buffer& buffer)
    : _impl(up::impl_make(buffer))
{ }

up_buffer_adapter::buffer_adapter::producer::operator FILE*()
{
    return _impl->get_file();
}
