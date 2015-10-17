#pragma once

#include "up_string_view.hpp"

namespace up_chunk
{

    class chunk final
    {
    public: // --- scope ---
        class into;
        class into_bulk_t;
        template <std::size_t N>
        class into_bulk_n;
        template <typename... Chunks>
        static auto into_bulk(Chunks&&... chunks);
        class from;
        class from_bulk_t;
        template <std::size_t N>
        class from_bulk_n;
        template <typename... Chunks>
        static auto from_bulk(Chunks&&... chunks);
    };


    class chunk::into final
    {
    private: // --- state ---
        char* _data;
        std::size_t _size;
    public: // --- life ---
        // implicit
        into(char* data, std::size_t size)
            : _data(std::move(data)), _size(std::move(size))
        { }
    public: // --- operations ---
        auto data() const -> auto { return _data; }
        auto size() const { return _size; }
        auto drain(std::size_t n) -> std::size_t;
    };


    class chunk::into_bulk_t
    {
    protected: // --- scope ---
        using self = into_bulk_t;
        static constexpr std::size_t Size = sizeof(char*) + sizeof(std::size_t);
    protected: // --- life ---
        explicit into_bulk_t() = default;
        ~into_bulk_t() noexcept = default;
        into_bulk_t(const self& rhs) = default;
        into_bulk_t(self&& rhs) noexcept = default;
    public: // --- operations ---
        auto count() const -> std::size_t;
        auto total() const -> std::size_t;
        auto head() const -> const into&;
        auto drain(std::size_t n) -> std::size_t
        {
            return _drain(n);
        }
        template <typename Type>
        auto as() -> Type*
        {
            static_assert(sizeof(Type) == Size, "size mismatch");
            static_assert(std::is_trivially_destructible<Type>::value, "non-trivial type");
            auto chunks = _chunks();
            Type* result = static_cast<Type*>(_raw_storage());
            for (std::size_t i = 0, j = _count(), k = 0; i != j; ++i) {
                if (chunks[i].size()) {
                    new (result + k) Type{chunks[i].data(), chunks[i].size()};
                    ++k;
                }
            }
            return result;
        }
    protected:
        auto operator=(const self& rhs) & -> self& = default;
        auto operator=(self&& rhs) & noexcept -> self& = default;
    private:
        virtual auto _count() const -> std::size_t = 0;
        virtual auto _chunks() const -> const into* = 0;
        virtual auto _drain(std::size_t n) -> std::size_t = 0;
        virtual auto _raw_storage() -> void* = 0;
    };


    template <std::size_t Count>
    class chunk::into_bulk_n final : public into_bulk_t
    {
    private: // --- scope ---
        template <typename Chunk>
        auto convert(Chunk&& chunk) -> into
        {
            static_assert(std::is_convertible<Chunk&&, into>::value, "expect implicit conversion");
            return std::forward<Chunk>(chunk);
        }
    private: // --- state ---
        into _into[Count];
        std::size_t _offset = 0;
        alignas(alignof(std::max_align_t)) char _storage[Count * Size];
    public: // --- life ---
        template <typename... Chunks>
        explicit into_bulk_n(Chunks&&... chunks)
            : _into{convert(std::forward<Chunks>(chunks))...}
        { }
    private: // --- operations ---
        auto _count() const -> std::size_t override
        {
            return Count - _offset;
        }
        auto _chunks() const -> const into* override
        {
            return _into + _offset;
        }
        auto _drain(std::size_t n) -> std::size_t override
        {
            for (std::size_t i = _offset; n && i != Count; ++i) {
                n = _into[i].drain(n);
            }
            while (_offset != Count && _into[_offset].size() == 0) {
                ++_offset;
            }
            return n;
        }
        auto _raw_storage() -> void* override
        {
            return _storage;
        }
    };


    template <typename... Chunks>
    auto chunk::into_bulk(Chunks&&... chunks)
    {
        return into_bulk_n<sizeof...(Chunks)>{std::forward<Chunks>(chunks)...};
    }


    class chunk::from final
    {
    private: // --- state ---
        const char* _data;
        std::size_t _size;
    public: // --- life ---
        // implicit (support brace initialization)
        from(const char* data, std::size_t size)
            : _data(std::move(data)), _size(std::move(size))
        { }
        explicit from(up::string_view value)
            : from(value.data(), value.size())
        { }
    public: // --- operations ---
        auto data() const { return _data; }
        auto size() const { return _size; }
        auto drain(std::size_t n) -> std::size_t;
    };


    class chunk::from_bulk_t
    {
    protected: // --- scope ---
        using self = from_bulk_t;
        static constexpr std::size_t Size = sizeof(char*) + sizeof(std::size_t);
    protected: // --- life ---
        explicit from_bulk_t() = default;
        ~from_bulk_t() noexcept = default;
        from_bulk_t(const self& rhs) = default;
        from_bulk_t(self&& rhs) noexcept = default;
    public: // --- operations ---
        auto count() const -> std::size_t;
        auto total() const -> std::size_t;
        auto head() const -> const from&;
        auto drain(std::size_t n) -> std::size_t
        {
            return _drain(n);
        }
        template <typename Type>
        auto as() -> Type*
        {
            static_assert(sizeof(Type) == Size, "size mismatch");
            static_assert(std::is_trivially_destructible<Type>::value, "non-trivial type");
            auto chunks = _chunks();
            Type* result = static_cast<Type*>(_raw_storage());
            for (std::size_t i = 0, j = _count(), k = 0; i != j; ++i) {
                if (chunks[i].size()) {
                    new (result + k) Type{const_cast<char*>(chunks[i].data()), chunks[i].size()};
                    ++k;
                }
            }
            return result;
        }
    protected:
        auto operator=(const self& rhs) & -> self& = default;
        auto operator=(self&& rhs) & noexcept -> self& = default;
    private:
        virtual auto _count() const -> std::size_t = 0;
        virtual auto _chunks() const -> const from* = 0;
        virtual auto _drain(std::size_t n) -> std::size_t = 0;
        virtual auto _raw_storage() -> void* = 0;
    };


    template <std::size_t Count>
    class chunk::from_bulk_n final : public from_bulk_t
    {
    private: // --- scope ---
        template <typename Chunk>
        auto convert(Chunk&& chunk) -> from
        {
            static_assert(std::is_convertible<Chunk&&, from>::value, "expect implicit conversion");
            return std::forward<Chunk>(chunk);
        }
    private: // --- state ---
        from _from[Count];
        std::size_t _offset = 0;
        alignas(alignof(std::max_align_t)) char _storage[Count * Size];
    public: // --- life ---
        template <typename... Chunks>
        explicit from_bulk_n(Chunks&&... chunks)
            : _from{convert(std::forward<Chunks>(chunks))...}
        { }
    private: // --- operations ---
        auto _count() const -> std::size_t override
        {
            return Count - _offset;
        }
        auto _chunks() const -> const from* override
        {
            return _from + _offset;
        }
        auto _drain(std::size_t n) -> std::size_t override
        {
            for (std::size_t i = _offset; n && i != Count; ++i) {
                n = _from[i].drain(n);
            }
            while (_offset != Count && _from[_offset].size() == 0) {
                ++_offset;
            }
            return n;
        }
        auto _raw_storage() -> void* override
        {
            return _storage;
        }
    };


    template <typename... Chunks>
    auto chunk::from_bulk(Chunks&&... chunks)
    {
        return from_bulk_n<sizeof...(Chunks)>{std::forward<Chunks>(chunks)...};
    }

}

namespace up
{

    using up_chunk::chunk;

}
