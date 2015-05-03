#include "up_zlib.hpp"

#include <zlib.h>

#include "up_char_cast.hpp"
#include "up_exception.hpp"
#include "up_integral_cast.hpp"
#include "up_terminate.hpp"


namespace
{

    using namespace up::literals;

    struct runtime;


    class processor
    {
    public: // --- scope ---
        using self = processor;
    public: // --- state ---
        z_stream _z_stream;
        up::buffer _buffer;
    public: // --- life ---
        explicit processor()
        {
            static_assert(Z_NULL == 0, "Z_NULL is legacy null pointer constant");
            _z_stream.zalloc = nullptr;
            _z_stream.zfree = nullptr;
            _z_stream.opaque = nullptr;
            _z_stream.avail_in = 0;
            _z_stream.next_in = nullptr;
        }
        processor(const self& rhs) = delete;
        processor(self&& rhs) noexcept = delete;
    protected:
        ~processor() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
    protected:
        template <typename Operation>
        void process_impl(
            Operation&& operation, up::string_literal&& message, up::chunk::from chunk, int flush)
        {
            _z_stream.avail_in = up::integral_caster(chunk.size());
            _z_stream.next_in = up::char_cast<Bytef>(const_cast<char*>(chunk.data()));
            int rv = 0;
            do {
                _buffer.reserve(chunk.size() / 4 + 256);
                _z_stream.avail_out = up::integral_caster(_buffer.capacity());
                _z_stream.next_out = reinterpret_cast<Bytef*>(_buffer.cold());
                rv = operation(&_z_stream, flush);
                if (rv != Z_OK && rv != Z_STREAM_END) {
                    UP_RAISE(runtime, message, rv);
                }
                _buffer.produce(_buffer.capacity() - _z_stream.avail_out);
            } while (_z_stream.avail_out == 0);
            if (_z_stream.avail_in > 0) {
                UP_RAISE(runtime, message, _z_stream.avail_in);
            }
            if (flush == Z_FINISH && rv != Z_STREAM_END) {
                UP_RAISE(runtime, message, rv);
            }
        }
    };

}


auto up_zlib::zlib::compress(up::chunk::from chunk) -> up::buffer
{
    return compressor()(chunk).finish();
}

auto up_zlib::zlib::decompress(up::chunk::from chunk) -> up::buffer
{
    return decompressor()(chunk).finish();
}


class up_zlib::zlib::compressor::impl final : public processor
{
public: // --- life ---
    explicit impl(int level)
    {
        int rv = ::deflateInit(&_z_stream, level);
        if (rv != Z_OK) {
            UP_RAISE(runtime, "zlib-bad-deflate"_s, rv);
        }
    }
    ~impl() noexcept
    {
        int rv = ::deflateEnd(&_z_stream);
        if (rv != Z_OK && rv != Z_DATA_ERROR) {
            UP_TERMINATE("zlib-bad-free"_s, rv);
        }
    }
public: // --- operations ---
    void process(up::chunk::from chunk, int flush)
    {
        process_impl(::deflate, "zlib-bad-deflate"_s, chunk, flush);
    }
};


up_zlib::zlib::compressor::compressor()
    : _impl(up::make_impl<impl>(Z_DEFAULT_COMPRESSION))
{ }

up_zlib::zlib::compressor::compressor(int level)
    : _impl(up::make_impl<impl>(level))
{ }

auto up_zlib::zlib::compressor::operator()(up::chunk::from chunk) -> self&
{
    _impl->process(chunk, Z_NO_FLUSH);
    return *this;
}

auto up_zlib::zlib::compressor::partial(bool flush) -> up::buffer
{
    if (flush) {
        _impl->process({nullptr, 0}, Z_FULL_FLUSH);
    }
    up::buffer result;
    result.swap(_impl->_buffer);
    return result;
}

auto up_zlib::zlib::compressor::finish() -> up::buffer
{
    _impl->process({nullptr, 0}, Z_FINISH);
    return std::move(_impl->_buffer);
}


class up_zlib::zlib::decompressor::impl final : public processor
{
public: // --- life ---
    explicit impl()
    {
        int rv = ::inflateInit(&_z_stream);
        if (rv != Z_OK) {
            UP_RAISE(runtime, "zlib-bad-inflate"_s, rv);
        }
    }
    ~impl() noexcept
    {
        int rv = ::inflateEnd(&_z_stream);
        if (rv != Z_OK) {
            UP_TERMINATE("zlib-bad-free"_s, rv);
        }
    }
public: // --- operations ---
    void process(up::chunk::from chunk, int flush)
    {
        process_impl(::inflate, "zlib-bad-inflate"_s, chunk, flush);
    }
};


up_zlib::zlib::decompressor::decompressor()
    : _impl(up::make_impl<impl>())
{ }

auto up_zlib::zlib::decompressor::operator()(up::chunk::from chunk) -> self&
{
    _impl->process(chunk, Z_NO_FLUSH);
    return *this;
}

auto up_zlib::zlib::decompressor::partial(bool flush) -> up::buffer
{
    if (flush) {
        _impl->process({nullptr, 0}, Z_FULL_FLUSH);
    }
    up::buffer result;
    result.swap(_impl->_buffer);
    return result;
}

auto up_zlib::zlib::decompressor::finish() -> up::buffer
{
    _impl->process({nullptr, 0}, Z_FINISH);
    return std::move(_impl->_buffer);
}


auto up_zlib::zlib::_compressv(up::chunk::from* chunks, std::size_t count) -> up::buffer
{
    compressor worker;
    for (std::size_t i = 0; i != count; ++i) {
        worker(chunks[i]);
    }
    return worker.finish();
}

auto up_zlib::zlib::_decompressv(up::chunk::from* chunks, std::size_t count) -> up::buffer
{
    decompressor worker;
    for (std::size_t i = 0; i != count; ++i) {
        worker(chunks[i]);
    }
    return worker.finish();
}
