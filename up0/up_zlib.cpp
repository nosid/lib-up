#include "up_zlib.hpp"

#include <zlib.h>

#include "up_char_cast.hpp"
#include "up_exception.hpp"
#include "up_ints.hpp"
#include "up_terminate.hpp"


namespace
{

    class processor
    {
    public: // --- scope ---
        using self = processor;
    public: // --- state ---
        z_stream _z_stream;
        up::buffer _buffer;
    protected: // --- life ---
        explicit processor()
        {
            static_assert(Z_NULL == 0);
            _z_stream.zalloc = nullptr;
            _z_stream.zfree = nullptr;
            _z_stream.opaque = nullptr;
            _z_stream.avail_in = 0;
            _z_stream.next_in = nullptr;
        }
        processor(const self& rhs) = delete;
        processor(self&& rhs) noexcept = delete;
        ~processor() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
    protected:
        template <typename Operation>
        void process_impl(
            Operation&& operation, const up::source& source, up::chunk::from chunk, int flush)
        {
            _z_stream.avail_in = up::ints::caster(chunk.size());
            _z_stream.next_in = up::char_cast<Bytef>(const_cast<char*>(chunk.data()));
            int rv = 0;
            do {
                _buffer.reserve(chunk.size() / 4 + 256);
                _z_stream.avail_out = up::ints::caster(_buffer.capacity());
                _z_stream.next_out = reinterpret_cast<Bytef*>(_buffer.cold());
                rv = operation(&_z_stream, flush);
                if (rv != Z_OK && rv != Z_STREAM_END) {
                    throw up::make_exception(source).with(rv);
                }
                _buffer.produce(_buffer.capacity() - _z_stream.avail_out);
            } while (_z_stream.avail_out == 0);
            if (_z_stream.avail_in > 0) {
                throw up::make_exception(source).with(_z_stream.avail_in);
            }
            if (flush == Z_FINISH && rv != Z_STREAM_END) {
                throw up::make_exception(source).with(rv);
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
            throw up::make_exception("zlib-bad-deflate").with(rv);
        }
    }
    ~impl() noexcept
    {
        int rv = ::deflateEnd(&_z_stream);
        if (rv != Z_OK && rv != Z_DATA_ERROR) {
            up::terminate("zlib-bad-free", rv);
        }
    }
public: // --- operations ---
    void process(up::chunk::from chunk, int flush)
    {
        process_impl(::deflate, "zlib-bad-deflate", chunk, flush);
    }
};


void up_zlib::zlib::compressor::destroy(impl* ptr)
{
    std::default_delete<impl>()(ptr);
}

up_zlib::zlib::compressor::compressor()
    : _impl(up::impl_make(Z_DEFAULT_COMPRESSION))
{ }

up_zlib::zlib::compressor::compressor(int level)
    : _impl(up::impl_make(level))
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
            throw up::make_exception("zlib-bad-inflate").with(rv);
        }
    }
    ~impl() noexcept
    {
        int rv = ::inflateEnd(&_z_stream);
        if (rv != Z_OK) {
            up::terminate("zlib-bad-free", rv);
        }
    }
public: // --- operations ---
    void process(up::chunk::from chunk, int flush)
    {
        process_impl(::inflate, "zlib-bad-inflate", chunk, flush);
    }
};


void up_zlib::zlib::decompressor::destroy(impl* ptr)
{
    std::default_delete<impl>()(ptr);
}

up_zlib::zlib::decompressor::decompressor()
    : _impl(up::impl_make())
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
