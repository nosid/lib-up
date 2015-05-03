#pragma once

#include "up_buffer.hpp"
#include "up_impl_ptr.hpp"

namespace up_zlib
{

    class zlib
    {
    public: // --- scope ---
        static auto compress(up::chunk::from chunk) -> up::buffer;
        template <typename... Chunks>
        static auto compressv(Chunks&&... chunks) -> up::buffer
        {
            up::chunk::from items[sizeof...(chunks)] = {{chunks.data(), chunks.size(), }...};
            return _compressv(items, sizeof...(chunks));
        }
        static auto decompress(up::chunk::from chunk) -> up::buffer;
        template <typename... Chunks>
        static auto decompressv(Chunks&&... chunks) -> up::buffer
        {
            up::chunk::from items[sizeof...(chunks)] = {{chunks.data(), chunks.size(), }...};
            return _decompressv(items, sizeof...(chunks));
        }
        class compressor;
        class decompressor;
    private:
        static auto _compressv(up::chunk::from* chunks, std::size_t count) -> up::buffer;
        static auto _decompressv(up::chunk::from* chunks, std::size_t count) -> up::buffer;
    };


    class zlib::compressor final
    {
    public: // --- scope ---
        using self = compressor;
        class impl;
    private: // --- state ---
        up::impl_ptr<impl> _impl;
    public: // --- life ---
        explicit compressor();
        explicit compressor(int level);
        compressor(const self& rhs) = delete;
        compressor(self&& rhs) noexcept = default;
        ~compressor() noexcept = default;
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
        auto operator()(up::chunk::from chunk) -> self&;
        auto partial(bool flush = false) -> up::buffer;
        auto finish() -> up::buffer;
    };


    class zlib::decompressor final
    {
    public: // --- scope ---
        using self = decompressor;
        class impl;
    private: // --- state ---
        up::impl_ptr<impl> _impl;
    public: // --- life ---
        explicit decompressor();
        decompressor(const self& rhs) = delete;
        decompressor(self&& rhs) noexcept = default;
        ~decompressor() noexcept = default;
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
        auto operator()(up::chunk::from chunk) -> self&;
        auto partial(bool flush = false) -> up::buffer;
        auto finish() -> up::buffer;
    };

}

namespace up
{

    using up_zlib::zlib;

}
