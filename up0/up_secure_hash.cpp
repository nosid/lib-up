#include "up_secure_hash.hpp"

#include "openssl/md5.h"
#include "openssl/sha.h"

#include "up_char_cast.hpp"
#include "up_exception.hpp"

/* Initialization of OpenSSL library: From the documentation of the OpenSSL
 * library, it is not clear whether the digest algorithms require a library
 * initialization before first use if used directly (in contrast to using the
 * EVP functions).
 *
 * A closer look at the digest implementations revealed nothing, that
 * indicated a need for initialization. The test programs shipped with the
 * OpenSSL library also perform no initialization. For these reasons, this
 * compilation unit performs no OpenSSL library initialization. */

namespace
{

    using shm = up_secure_hash::secure_hash_mechanism;

    template <shm Mechanism>
    struct raw_context final { };

    template <>
    struct raw_context<shm::md5> final
    {
        static_assert(secure_hash_digest_size(shm::md5) == MD5_DIGEST_LENGTH);
        MD5_CTX _ctx;
        auto init() { return MD5_Init(&_ctx); }
        auto update(const char* data, std::size_t size) { return MD5_Update(&_ctx, data, size); }
        auto final(unsigned char* data) { return MD5_Final(data, &_ctx); }
    };

    template <>
    struct raw_context<shm::sha1> final
    {
        static_assert(secure_hash_digest_size(shm::sha1) == SHA_DIGEST_LENGTH);
        SHA_CTX _ctx;
        auto init() { return SHA1_Init(&_ctx); }
        auto update(const char* data, std::size_t size) { return SHA1_Update(&_ctx, data, size); }
        auto final(unsigned char* data) { return SHA1_Final(data, &_ctx); }
    };

    template <>
    struct raw_context<shm::sha224> final
    {
        static_assert(secure_hash_digest_size(shm::sha224) == SHA224_DIGEST_LENGTH);
        SHA256_CTX _ctx;
        auto init() { return SHA224_Init(&_ctx); }
        auto update(const char* data, std::size_t size) { return SHA224_Update(&_ctx, data, size); }
        auto final(unsigned char* data) { return SHA224_Final(data, &_ctx); }
    };

    template <>
    struct raw_context<shm::sha256> final
    {
        static_assert(secure_hash_digest_size(shm::sha256) == SHA256_DIGEST_LENGTH);
        SHA256_CTX _ctx;
        auto init() { return SHA256_Init(&_ctx); }
        auto update(const char* data, std::size_t size) { return SHA256_Update(&_ctx, data, size); }
        auto final(unsigned char* data) { return SHA256_Final(data, &_ctx); }
    };

    template <>
    struct raw_context<shm::sha384> final
    {
        static_assert(secure_hash_digest_size(shm::sha384) == SHA384_DIGEST_LENGTH);
        SHA512_CTX _ctx;
        auto init() { return SHA384_Init(&_ctx); }
        auto update(const char* data, std::size_t size) { return SHA384_Update(&_ctx, data, size); }
        auto final(unsigned char* data) { return SHA384_Final(data, &_ctx); }
    };

    template <>
    struct raw_context<shm::sha512> final
    {
        static_assert(secure_hash_digest_size(shm::sha512) == SHA512_DIGEST_LENGTH);
        SHA512_CTX _ctx;
        auto init() { return SHA512_Init(&_ctx); }
        auto update(const char* data, std::size_t size) { return SHA512_Update(&_ctx, data, size); }
        auto final(unsigned char* data) { return SHA512_Final(data, &_ctx); }
    };


    template <shm Mechanism>
    void do_secure_hash(up::chunk::from* chunks, std::size_t count, up::chunk::into result)
    {
        if (secure_hash_digest_size(Mechanism) != result.size()) {
            throw up::make_exception("invalid-secure-hash-size").with(Mechanism, result.size());
        }
        raw_context<Mechanism> context;
        if (context.init() != 1) {
            throw up::make_exception("bad-secure-hash-init").with(Mechanism);
        }
        for (std::size_t i = 0; i < count; ++i) {
            if (context.update(chunks[i].data(), chunks[i].size()) != 1) {
                throw up::make_exception("bad-secure-hash-update").with(Mechanism);
            }
        }
        if (context.final(up::char_cast<unsigned char>(result.data())) != 1) {
            throw up::make_exception("bad-secure-hash-final").with(Mechanism);
        }
    }

}


auto up_secure_hash::to_string(secure_hash_mechanism mechanism) -> up::unique_string
{
    switch (mechanism) {
    case secure_hash_mechanism::md5: return "md5";
    case secure_hash_mechanism::sha1: return "sha1";
    case secure_hash_mechanism::sha224: return "sha224";
    case secure_hash_mechanism::sha256: return "sha256";
    case secure_hash_mechanism::sha384: return "sha384";
    case secure_hash_mechanism::sha512: return "sha512";
    default:
        throw up::make_exception("invalid-secure-hash-mechanism").with(mechanism);
    }
}


void up_secure_hash::secure_hash_aux(secure_hash_mechanism mechanism, up::chunk::from* chunks, std::size_t count, up::chunk::into result)
{
    switch (mechanism) {
    case secure_hash_mechanism::md5:
        do_secure_hash<secure_hash_mechanism::md5>(chunks, count, result);
        break;
    case secure_hash_mechanism::sha1:
        do_secure_hash<secure_hash_mechanism::sha1>(chunks, count, result);
        break;
    case secure_hash_mechanism::sha224:
        do_secure_hash<secure_hash_mechanism::sha224>(chunks, count, result);
        break;
    case secure_hash_mechanism::sha256:
        do_secure_hash<secure_hash_mechanism::sha256>(chunks, count, result);
        break;
    case secure_hash_mechanism::sha384:
        do_secure_hash<secure_hash_mechanism::sha384>(chunks, count, result);
        break;
    case secure_hash_mechanism::sha512:
        do_secure_hash<secure_hash_mechanism::sha512>(chunks, count, result);
        break;
    default:
        throw up::make_exception("invalid-secure-hash-mechanism").with(mechanism);
    }
}


auto up_secure_hash::secure_hash(secure_hash_mechanism mechanism, up::chunk::from chunk)
    -> secure_hash_digest
{
    secure_hash_digest result(secure_hash_digest_size(mechanism));
    secure_hash_aux(mechanism, &chunk, 1, result);
    return result;
}


class up_secure_hash::secure_hasher_aux::impl
{
public: // --- scope ---
    using self = impl;
    template <secure_hash_mechanism Mechanism>
    class derived;
protected: // --- life ---
    explicit impl() noexcept = default;
    impl(const self& rhs) = delete;
    impl(self&& rhs) noexcept = delete;
public:
    virtual ~impl() noexcept = default;
public: // --- operations ---
    auto operator=(const self& rhs) & -> self& = delete;
    auto operator=(self&& rhs) & noexcept -> self& = delete;
    virtual void update(up::chunk::from chunk) = 0;
    virtual void finish(up::chunk::into result) = 0;
};


template <up_secure_hash::secure_hash_mechanism Mechanism>
class up_secure_hash::secure_hasher_aux::impl::derived final : public impl
{
private: // --- state ---
    raw_context<Mechanism> _context;
public: // --- life ---
    explicit derived()
    {
        if (_context.init() != 1) {
            throw up::make_exception("bad-secure-hash-init").with(Mechanism);
        }
    }
private: // --- operations ---
    void update(up::chunk::from chunk) override
    {
        if (_context.update(chunk.data(), chunk.size()) != 1) {
            throw up::make_exception("bad-secure-hash-update").with(Mechanism);
        }
    }
    void finish(up::chunk::into result) override
    {
        if (secure_hash_digest_size(Mechanism) != result.size()) {
            throw up::make_exception("invalid-secure-hash-size").with(Mechanism, result.size());
        }
        if (_context.final(up::char_cast<unsigned char>(result.data())) != 1) {
            throw up::make_exception("bad-secure-hash-final").with(Mechanism);
        }
    }
};



void up_secure_hash::secure_hasher_aux::destroy(impl* ptr)
{
    std::default_delete<impl>()(ptr);
}

up_secure_hash::secure_hasher_aux::secure_hasher_aux(secure_hash_mechanism mechanism)
    : _impl()
{
    switch (mechanism) {
    case secure_hash_mechanism::md5:
        _impl = up::impl_make<impl::derived<secure_hash_mechanism::md5>>();
        break;
    case secure_hash_mechanism::sha1:
        _impl = up::impl_make<impl::derived<secure_hash_mechanism::sha1>>();
        break;
    case secure_hash_mechanism::sha224:
        _impl = up::impl_make<impl::derived<secure_hash_mechanism::sha224>>();
        break;
    case secure_hash_mechanism::sha256:
        _impl = up::impl_make<impl::derived<secure_hash_mechanism::sha256>>();
        break;
    case secure_hash_mechanism::sha384:
        _impl = up::impl_make<impl::derived<secure_hash_mechanism::sha384>>();
        break;
    case secure_hash_mechanism::sha512:
        _impl = up::impl_make<impl::derived<secure_hash_mechanism::sha512>>();
        break;
    default:
        throw up::make_exception("invalid-secure-hash-mechanism").with(mechanism);
    }
}

auto up_secure_hash::secure_hasher_aux::update(up::chunk::from chunk) -> self&
{
    _impl->update(chunk);
    return *this;
}

void up_secure_hash::secure_hasher_aux::finish(up::chunk::into result)
{
    _impl->finish(result);
}
