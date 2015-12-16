#pragma once

#include "up_chunk.hpp"
#include "up_impl_ptr.hpp"
#include "up_swap.hpp"

namespace up_secure_hash
{

    enum class secure_hash_mechanism { md5, sha1, sha224, sha256, sha384, sha512, };

    auto to_string(secure_hash_mechanism mechanism) -> std::string;

    constexpr auto secure_hash_digest_size(secure_hash_mechanism mechanism) -> std::size_t
    {
        return mechanism == secure_hash_mechanism::md5 ? 16
            : mechanism == secure_hash_mechanism::sha1 ? 20
            : mechanism == secure_hash_mechanism::sha224 ? 28
            : mechanism == secure_hash_mechanism::sha256 ? 32
            : mechanism == secure_hash_mechanism::sha384 ? 48
            : mechanism == secure_hash_mechanism::sha512 ? 64
            : 0;
    }

    class secure_hash_digest final
    {
    public: // --- scope ---
        using self = secure_hash_digest;
    private: // --- state ---
        std::size_t _size;
        std::unique_ptr<char[]> _data;
    public: // --- life ---
        explicit secure_hash_digest(std::size_t size)
            : _size(size), _data(new char[size])
        { }
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_size, rhs._size);
            up::swap_noexcept(_data, rhs._data);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto data() const noexcept -> const char*
        {
            return _data.get();
        }
        auto size() const noexcept -> std::size_t
        {
            return _size;
        }
        operator up::chunk::from() const
        {
            return up::chunk::from(_data.get(), _size);
        }
        operator up::chunk::into()
        {
            return up::chunk::into(_data.get(), _size);
        }
    };


    void secure_hash_aux(secure_hash_mechanism mechanism, up::chunk::from* chunks, std::size_t count, up::chunk::into result);

    auto secure_hash(secure_hash_mechanism mechanism, up::chunk::from chunk)
    {
        secure_hash_digest result(secure_hash_digest_size(mechanism));
        secure_hash_aux(mechanism, &chunk, 1, result);
        return result;
    }

    template <typename... Chunks>
    auto secure_hashv(secure_hash_mechanism mechanism, Chunks&&... chunks)
    {
        up::chunk::from items[sizeof...(chunks)] = {{chunks.data(), chunks.size(), }...};
        secure_hash_digest result(secure_hash_digest_size(mechanism));
        secure_hash_aux(mechanism, items, sizeof...(chunks), result);
        return result;
    }


    class secure_hasher_aux final
    {
    public: // --- scope ---
        using self = secure_hasher_aux;
        class impl;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, destroy> _impl;
    public: // --- life --
        explicit secure_hasher_aux(secure_hash_mechanism mechanism);
        secure_hasher_aux(const self&) = delete;
        secure_hasher_aux(self&& rhs) noexcept = default;
        ~secure_hasher_aux() noexcept = default;
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
        auto update(up::chunk::from chunk) -> self&;
        void finish(up::chunk::into result);
    };


    class secure_hasher final
    {
    public: // --- scope ---
        using self = secure_hasher;
    private: // --- state ---
        secure_hash_mechanism _mechanism;
        secure_hasher_aux _aux;
    public: // --- life --
        explicit secure_hasher(secure_hash_mechanism mechanism)
            : _mechanism(mechanism), _aux(mechanism)
        { }
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_mechanism, rhs._mechanism);
            up::swap_noexcept(_aux, rhs._aux);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto update(up::chunk::from chunk) -> secure_hasher&
        {
            _aux.update(chunk);
            return *this;
        }
        auto finish() -> secure_hash_digest
        {
            secure_hash_digest result(secure_hash_digest_size(_mechanism));
            _aux.finish(result);
            return result;
        }
    };


    template <secure_hash_mechanism Mechanism>
    class secure_hash_algorithm final
    {
    public: // --- scope ---
        class digest final
        {
        public: // --- scope ---
            using self = digest;
        private: // --- state ---
            std::array<char, secure_hash_digest_size(Mechanism)> _data;
        public: // --- operations ---
            void swap(self& rhs) noexcept
            {
                up::swap_noexcept(_data, rhs._data);
            }
            friend void swap(self& lhs, self& rhs) noexcept
            {
                lhs.swap(rhs);
            }
            auto data() const noexcept -> const char*
            {
                return _data.data();
            }
            constexpr auto size() const noexcept -> std::size_t
            {
                return secure_hash_digest_size(Mechanism);
            }
            operator up::chunk::from() const
            {
                return up::chunk::from(_data.data(), secure_hash_digest_size(Mechanism));
            }
            operator up::chunk::into()
            {
                return up::chunk::into(_data.data(), secure_hash_digest_size(Mechanism));
            }
        };
        class hasher final
        {
        public: // --- scope ---
            using self = hasher;
        private: // --- state ---
            secure_hasher_aux _aux;
        public: // --- life --
            explicit hasher()
                : _aux(Mechanism)
            { }
        public: // --- operations ---
            void swap(self& rhs) noexcept
            {
                up::swap_noexcept(_aux, rhs._aux);
            }
            friend void swap(self& lhs, self& rhs) noexcept
            {
                lhs.swap(rhs);
            }
            auto update(up::chunk::from chunk) -> hasher&
            {
                _aux.update(chunk);
                return *this;
            }
            auto finish() -> digest
            {
                digest result;
                _aux.finish(result);
                return result;
            }
        };
        static auto hash(up::chunk::from chunk) -> digest
        {
            digest result;
            secure_hash_aux(Mechanism, &chunk, 1, result);
            return result;
        }
        template <typename... Chunks>
        static auto hashv(Chunks&&... chunks) -> digest
        {
            up::chunk::from items[sizeof...(chunks)] = {{chunks.data(), chunks.size(), }...};
            digest result;
            secure_hash_aux(Mechanism, items, sizeof...(chunks), result);
            return result;
        }
    };


    using md5 = secure_hash_algorithm<secure_hash_mechanism::md5>;
    using sha1 = secure_hash_algorithm<secure_hash_mechanism::sha1>;
    using sha224 = secure_hash_algorithm<secure_hash_mechanism::sha224>;
    using sha256 = secure_hash_algorithm<secure_hash_mechanism::sha256>;
    using sha384 = secure_hash_algorithm<secure_hash_mechanism::sha384>;
    using sha512 = secure_hash_algorithm<secure_hash_mechanism::sha512>;

}

namespace up
{

    using up_secure_hash::secure_hash_digest_size;
    using up_secure_hash::secure_hash_digest;

    using up_secure_hash::secure_hash;
    using up_secure_hash::secure_hashv;
    using up_secure_hash::secure_hasher;

    using up_secure_hash::md5;
    using up_secure_hash::sha1;
    using up_secure_hash::sha224;
    using up_secure_hash::sha256;
    using up_secure_hash::sha384;
    using up_secure_hash::sha512;

}
