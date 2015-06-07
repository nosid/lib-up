#pragma once

/**
 * @file
 *
 * @brief Header file providing a simple and efficient interface to the zlib
 * library.
 */

#include "up_buffer.hpp"
#include "up_impl_ptr.hpp"

/// Designated namespace for the header file.
namespace up_zlib
{

    /**
     * @brief The class is only used to bundle zlib related stuff together.
     */
    class zlib
    {
    public: // --- scope ---
        /**
         * @brief The function compresses the binary string given by @c chunk.
         *
         * The compression happens at once, and the compressed result is
         * returned. It uses the default compression level. The function
         * covers the most common use case for compression. However, there are
         * alternatives for compressing a binary string consisting of several
         * chunks, and for incremental compression.
         */
        static auto compress(up::chunk::from chunk) -> up::buffer;
        /**
         * @brief The function compresses the binary string consisting of the
         * given @c chunks.
         *
         * The compression happens at once, and the compressed result is
         * returned. It uses the default compression level. It is a
         * convenience function and might be slightly faster than using the @c
         * compressor for incremental compression.
         */
        template <typename... Chunks>
        static auto compressv(Chunks&&... chunks) -> up::buffer
        {
            up::chunk::from items[sizeof...(chunks)] = {{chunks.data(), chunks.size(), }...};
            return _compressv(items, sizeof...(chunks));
        }
        /**
         * @brief The function decompresses the binary string given by @c
         * chunk.
         *
         * The decompression happens at once, and the decompressed result is
         * returned. The function covers the most common use case for
         * decompression. However, there are alternatives for decompressing a
         * binary string consisting of several chunks, and for incremental
         * decompression.
         *
         * If an error occurs, an undetermined exception is thrown.
         */
        static auto decompress(up::chunk::from chunk) -> up::buffer;
        /**
         * @brief The function decompresses the binary string consisting of
         * the given @c chunks.
         *
         * The decompression happens at once, and the decompressed result is
         * returned. It is a convenience function and might be slightly faster
         * than using the @c decompressor for incremental decompression.
         *
         * If an error occurs, an undetermined exception is thrown.
         */
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


    /**
     * @brief The class provides the full functionality for compressing binary
     * string, in particular specifying a compression level and incremental
     * processing.
     */
    class zlib::compressor final
    {
    private: // --- scope ---
        using self = compressor;
        class impl;
    private: // --- state ---
        up::impl_ptr<impl> _impl;
    public: // --- life ---
        /// Construct instance with default compression level.
        explicit compressor();
        /// Construct instance with given compression level @c level.
        explicit compressor(int level);
        /// @private
        compressor(const self& rhs) = delete;
        /// Move constructor with standard behavior.
        compressor(self&& rhs) noexcept = default;
        /**
         * @brief Destructor with standard behavior (silently discarding
         * remaining data).
         */
        ~compressor() noexcept = default;
    public: // --- operations ---
        /// @private
        auto operator=(const self& rhs) & -> self& = delete;
        /// Move assignment operator with standard behavior.
        auto operator=(self&& rhs) & noexcept -> self& = default;
        /// Swap function with standard behavior.
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        /// Swap function with standard behavior.
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        /**
         * @brief The function pushes additional data given by @c chunk to the
         * compressor.
         *
         * The data is completely processed, i.e. there are no partial
         * results. The compressed data is not directly returned, but stored
         * internally. No checkpoint is created within the compressed stream
         * (unless @c partial or @c finish is called).
         */
        auto operator()(up::chunk::from chunk) -> self&;
        /**
         * @brief The function returns some of the compressed data, that was
         * not returned before.
         *
         * Optionally, a checkpoint can be created before returning the
         * result.
         */
        auto partial(bool flush = false) -> up::buffer;
        /**
         * @brief The function finishes the compression and returns all
         * remaining compressed data. After invoking this function, the
         * instance should not be used any longer.
         */
        auto finish() -> up::buffer;
    };


    /**
     * @brief The class provides the full functionality for decompressing
     * binary string, in particular incremental processing.
     */
    class zlib::decompressor final
    {
    private: // --- scope ---
        using self = decompressor;
        class impl;
    private: // --- state ---
        up::impl_ptr<impl> _impl;
    public: // --- life ---
        /// Construct instance.
        explicit decompressor();
        /// @private
        decompressor(const self& rhs) = delete;
        /// Move constructor with standard behavior.
        decompressor(self&& rhs) noexcept = default;
        ~decompressor() noexcept = default;
    public: // --- operations ---
        /// @private
        auto operator=(const self& rhs) & -> self& = delete;
        /// Move assignment operator with standard behavior.
        auto operator=(self&& rhs) & noexcept -> self& = default;
        /// Swap function with standard behavior.
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        /// Swap function with standard behavior.
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        /**
         * @brief The function pushes additional data given by @c chunk to the
         * decompressor.
         *
         * The data is completely processed, i.e. there are no partial
         * results. The decompressed data is not directly returned, but stored
         * internally.
         *
         * If an error occurs, an undetermined exception is thrown.
         */
        auto operator()(up::chunk::from chunk) -> self&;
        /**
         * @brief The function returns some of the decompressed data, that was
         * not returned before.
         *
         * If an error occurs, an undetermined exception is thrown.
         */
        auto partial(bool flush = false) -> up::buffer;
        /**
         * @brief The function finishes the decompression and returns all
         * remaining decompressed data. After invoking this function, the
         * instance should not be used any longer.
         *
         * If an error occurs, an undetermined exception is thrown.
         */
        auto finish() -> up::buffer;
    };

}

namespace up
{

    using up_zlib::zlib;

}
