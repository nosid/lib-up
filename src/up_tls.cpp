#include "up_tls.hpp"

#include <atomic>
#include <cstring>
#include <mutex>

/* For an introduction to openssl, see the man page ssl(3ssl). It contains
 * an overview of the most important API functions. */
#include "openssl/conf.h"
#include "openssl/engine.h"
#include "openssl/err.h"
#include "openssl/ssl.h"

#include "up_buffer_adapter.hpp"
#include "up_exception.hpp"
#include "up_integral_cast.hpp"
#include "up_utility.hpp"

namespace
{

    using namespace up::literals;

    struct runtime { };


    class openssl_process final
    {
    public: // --- scope ---
        using self = openssl_process;
        static auto instance() -> auto&
        {
            static self instance;
            return instance;
        }
    private:
        static void _put_thread_id(CRYPTO_THREADID* result, void* value)
        {
            ::CRYPTO_THREADID_set_pointer(result, value);
        }
        template <typename Integer,
                  typename = typename std::enable_if<std::is_unsigned<Integer>::value>::type>
        static void _put_thread_id(CRYPTO_THREADID* result, Integer value)
        {
            unsigned long v{value};
            ::CRYPTO_THREADID_set_numeric(result, v);
        }
        static void _thread_id_callback(CRYPTO_THREADID* result)
        {
            _put_thread_id(result, ::pthread_self());
        }
        static void _locking_callback(int mode, int type,
            const char* file __attribute__((unused)), int line __attribute__((unused)))
        {
            if (mode & CRYPTO_LOCK) {
                instance()._mutexes[type].lock();
            } else {
                instance()._mutexes[type].unlock();
            }
        }
    private: // --- state ---
        std::unique_ptr<std::mutex[]> _mutexes;
    public: // --- life ---
        explicit openssl_process()
            : _mutexes(std::make_unique<std::mutex[]>(CRYPTO_num_locks()))
        {
            // at first the SSL library must be initialized
            ::SSL_library_init();
            /* The following operation is optional, but provides better error
             * messages. The function implicitly also calls
             * ERR_load_crypto_strings(). */
            ::SSL_load_error_strings();
            ::OpenSSL_add_all_algorithms();
            // required for multi-threaded programs
            ::CRYPTO_THREADID_set_callback(&_thread_id_callback);
            ::CRYPTO_set_locking_callback(&_locking_callback);
        }
        ~openssl_process() noexcept
        {
            ::CRYPTO_set_locking_callback(nullptr);
            ::CRYPTO_THREADID_set_callback(nullptr);
            // application-global cleanup functions (see OpenSSL FAQ)
            ::ENGINE_cleanup();
            ::CONF_modules_unload(1);
            ::ERR_free_strings();
            ::EVP_cleanup();
            ::CRYPTO_cleanup_all_ex_data();
        }
        openssl_process(const self& rhs) = delete;
        openssl_process(self&& rhs) noexcept = delete;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
    };


    class openssl_thread final
    {
    public: // --- scope ---
        using self = openssl_thread;
        static auto instance() -> auto&
        {
            static UP_WORKAROUND_THREAD_LOCAL self instance;
            return instance;
        }
    public: // --- life ---
        explicit openssl_thread()
        {
            openssl_process::instance();
        }
        ~openssl_thread() noexcept
        {
            /* Thread-local cleanup: Free the error queue associated with
             * current . Since error queue data structures are allocated
             * automatically for new threads, they must be freed when threads
             * are terminated in order to avoid memory leaks. */
            ::ERR_remove_state(0);
        }
        openssl_thread(const self& rhs) = delete;
        openssl_thread(self&& rhs) noexcept = delete;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
    };


    template <typename... Args>
    [[noreturn]]
    void raise_ssl_error(up::string_literal message, Args&&... args)
    {
        if (auto code = ::ERR_get_error()) {
            /* Throw first error and discard all others silently. */
            auto lib = ::ERR_lib_error_string(code);
            auto func = ::ERR_lib_error_string(code);
            auto reason = ::ERR_reason_error_string(code);
            ::ERR_clear_error();
            UP_RAISE(runtime, message, code, lib, func, reason, std::forward<Args>(args)...);
        } else {
            UP_RAISE(runtime, message, std::forward<Args>(args)...);
        }
    }


    class x509 final
    {
    private: // --- scope ---
        using self = x509;
    private: // --- state ---
        X509* _ptr;
    public: // --- life ---
        explicit x509(const up::buffer& buffer)
            : x509()
        {
            _ptr = ::PEM_read_X509(up::buffer_adapter::reader(buffer), nullptr, nullptr, nullptr);
            if (_ptr == nullptr) {
                raise_ssl_error("bad-tls-certificate-error"_s);
            }
        }
        x509(const self& rhs) = delete;
        x509(self&& rhs) noexcept = delete;
        ~x509() noexcept
        {
            if (_ptr) {
                ::X509_free(_ptr);
            }
        }
    private:
        explicit x509()
            : _ptr(nullptr)
        { }
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
        auto native_handle() const { return _ptr; }
    };


    class bio_adapter final
    {
    public: // --- scope ---
        static BIO_METHOD methods;
    private:
        static int _bwrite(BIO* bio, const char* data, int size)
        {
            using engine = up::stream::engine;
            engine* stream = static_cast<engine*>(bio->ptr);
            try {
                ::BIO_clear_retry_flags(bio);
                return up::integral_caster(
                    stream->write_some({data, up::integral_caster(size)}));
            } catch (const up::exception<engine::unreadable>&) {
                ::BIO_set_retry_read(bio);
                return -1;
            } catch (const up::exception<engine::unwritable>&) {
                ::BIO_set_retry_write(bio);
                return -1;
            } catch (...) {
                return -1;
            }
        }
        static int _bread(BIO* bio, char* data, int size)
        {
            using engine = up::stream::engine;
            engine* stream = static_cast<engine*>(bio->ptr);
            try {
                ::BIO_clear_retry_flags(bio);
                return up::integral_caster(
                    stream->read_some({data, up::integral_caster(size)}));
            } catch (const up::exception<engine::unreadable>&) {
                ::BIO_set_retry_read(bio);
                return -1;
            } catch (const up::exception<engine::unwritable>&) {
                ::BIO_set_retry_write(bio);
                return -1;
            } catch (...) {
                return -1;
            }
        }
        static int _bputs(BIO* bio, const char* data)
        {
            return _bwrite(bio, data, std::strlen(data));
        }
        static long _ctrl(BIO* bio, int cmd, long num, void* ptr __attribute__((unused)))
        {
            switch (cmd) {
            case BIO_CTRL_GET_CLOSE:
                return bio->shutdown;
            case BIO_CTRL_SET_CLOSE:
                bio->shutdown = up::integral_caster(num);
                return 1;
            case BIO_CTRL_DUP:
                return 1;
            case BIO_CTRL_FLUSH:
                return 1;
            default:
                return 0;
            }
        }
        static int _create(BIO* bio)
        {
            bio->init = 0;
            bio->flags = 0;
            bio->num = 0;
            bio->ptr = nullptr;
            return 1;
        }
        static int _destroy(BIO* bio)
        {
            bio->init = 0;
            bio->flags = 0;
            bio->ptr = nullptr;
            return 1;
        }
    };

    BIO_METHOD bio_adapter::methods = {
        0x42 | BIO_TYPE_SOURCE_SINK, // arbitrary, unused type
        "tls bio adapter", // name
        &_bwrite,
        &_bread,
        &_bputs,
        nullptr, // bgets
        &_ctrl,
        &_create,
        &_destroy,
        nullptr, // callback_ctrl
    };


    class spin_lock final
    {
    private: // --- scope ---
        using self = spin_lock;
    private: // --- state ---
        std::atomic_flag& _flag;
    public: // --- life ---
        explicit spin_lock(std::atomic_flag& flag)
            : _flag(flag)
        {
            while (_flag.test_and_set()) { }
        }
        spin_lock(const self& rhs) = delete;
        spin_lock(self&& rhs) noexcept = delete;
        ~spin_lock() noexcept
        {
            _flag.clear();
        }
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
    };


    class tls_stream final : public up::stream::engine
    {
    public: // --- scope ---
        using self = tls_stream;
        using await = up::stream::await;
        enum class state { good, bad, read_in_progress, write_in_progress, shutdown_in_progress, };
        /* The OpenSSL functions behave a bit strange when it comes to errors.
         * Apparently, even if a function returns with an error, the function
         * call might have changed something. According to the documentation,
         * the last operation has to be retried before another operation can
         * be used. The sentry class protects the stream from concurrent
         * access, and tracks the state to identify this kind of misuse. */
        class sentry final
        {
        public: // -- scope ---
            using self = sentry;
        private: // --- state ---
            const tls_stream* _owner;
            spin_lock _lock;
        public: // --- life ---
            explicit sentry(const tls_stream* owner, state expected)
                : _owner(owner), _lock(_owner->_lock)
            {
                if (_owner->_state != state::good && _owner->_state != expected) {
                    UP_RAISE(runtime, "tls-bad-state"_s,
                        up::to_underlying_type(_owner->_state),
                        up::to_underlying_type(expected));
                } else {
                    _owner->_state = expected;
                }
            }
        };
        static auto make_ssl(SSL_CTX* ctx)
        {
            openssl_thread::instance();
            return std::unique_ptr<SSL, decltype(&::SSL_free)>{::SSL_new(ctx), &::SSL_free};
        }
    private: // --- state ---
        up::impl_ptr<engine> _underlying;
        std::unique_ptr<SSL, decltype(&::SSL_free)> _ssl;
        /* Spin lock: Apparently, OpenSSL is not thread-safe in the same way
         * as POSIX socket. That means, you can't read from and write to the
         * same TLS stream at the same time. The lock is used to protect
         * against this kind of use. A spin lock is used, because the stream
         * should not be used that way, so it causes basically no overhead. */
        mutable std::atomic_flag _lock = ATOMIC_FLAG_INIT;
        mutable state _state = state::bad;
    public: // --- life ---
        explicit tls_stream(SSL_CTX* ctx, up::impl_ptr<engine> underlying, await& awaiting)
            : _underlying(std::move(underlying)), _ssl(make_ssl(ctx))
        {
            if (_ssl == nullptr) {
                raise_ssl_error("bad-tls-error"_s);
            }
            /* User-defined BIO. Note that if the BIO is associated with
             * SSL, it is automatically freed in SSL_free. */
            BIO* bio = ::BIO_new(&bio_adapter::methods);
            bio->ptr = _underlying.get();
            bio->init = 1;
            ::SSL_set_bio(_ssl.get(), bio, bio);
            for (;;) {
                auto result = ::SSL_connect(_ssl.get()); // XXX: support accept mode with ::SSL_accept(...)
                if (result == 1) {
                    /* OK. The TLS/SSL handshake was successfully completed, a
                     * TLS/SSL connection has been established. */
                    break;
                } else if (result == 0) {
                    /* The TLS/SSL handshake was not successful but was shut
                     * down controlled and by the specifications of the
                     * TLS/SSL protocol. */
                    raise_ssl_error("tls-handshake-error"_s,
                        result, ::SSL_get_error(_ssl.get(), result));
                } else {
                    auto error = ::SSL_get_error(_ssl.get(), result);
                    if (error == SSL_ERROR_WANT_READ) {
                        awaiting(_underlying->get_native_handle(), await::operation::read);
                    } else if (error == SSL_ERROR_WANT_WRITE) {
                        awaiting(_underlying->get_native_handle(), await::operation::write);
                    } else {
                        raise_ssl_error("bad-ssl-io-error"_s, result, error);
                    }
                }
            }
            _state = state::good;
        }
    private: // --- operations ---
        void shutdown() const override
        {
            sentry sentry(this, state::shutdown_in_progress);
            openssl_thread::instance();
            while (int result = ::SSL_shutdown(_ssl.get()) != 1) {
                if (result == 0) {
                    // restart
                } else {
                    auto error = ::SSL_get_error(_ssl.get(), result);
                    if (error == SSL_ERROR_WANT_READ) {
                        UP_RAISE(unreadable, "unreadable-tls-stream"_s);
                    } else if (error == SSL_ERROR_WANT_WRITE) {
                        UP_RAISE(unwritable, "unwritable-tls-stream"_s);
                    } else {
                        _state = state::bad;
                        raise_ssl_error("bad-ssl-io-error"_s, result, error);
                    }
                }
            }
            _underlying->shutdown();
            _state = state::good;
        }
        void hard_close() const override
        {
            _underlying->hard_close();
        }
        auto read_some(up::chunk::into chunk) const -> std::size_t override
        {
            sentry sentry(this, state::read_in_progress);
            openssl_thread::instance();
            return _handle_io_result(
                ::SSL_read(_ssl.get(), chunk.data(), up::integral_caster(chunk.size())), true);
        }
        auto write_some(up::chunk::from chunk) const -> std::size_t override
        {
            sentry sentry(this, state::write_in_progress);
            openssl_thread::instance();
            return _handle_io_result(
                ::SSL_write(_ssl.get(), chunk.data(), up::integral_caster(chunk.size())), false);
        }
        auto read_some(up::chunk::into_bulk_t& chunks) const -> std::size_t override
        {
            /* Unfortunately, OpenSSL has no support for multiple buffers. So,
             * we only process the first non-empty buffer. */
            for (std::size_t i = 0, j = chunks.count(); i != j; ++i) {
                auto&& chunk = chunks[i];
                if (chunk.size()) {
                    return read_some(chunk);
                } // else: check next
            }
            UP_RAISE(runtime, "tls-bad-read-chunks"_s, chunks.total(), chunks.count());
        }
        auto write_some(up::chunk::from_bulk_t& chunks) const -> std::size_t override
        {
            /* Unfortunately, OpenSSL has no support for multiple buffers. So,
             * we only process the first non-empty buffer. */
            for (std::size_t i = 0, j = chunks.count(); i != j; ++i) {
                auto&& chunk = chunks[i];
                if (chunk.size()) {
                    return write_some(chunk);
                } // else: check next
            }
            UP_RAISE(runtime, "tls-bad-write-chunks"_s, chunks.total(), chunks.count());
        }
        auto get_underlying_engine() const -> const engine* override
        {
            return _underlying->get_underlying_engine();
        }
        auto get_native_handle() const -> up::stream::native_handle override
        {
            return _underlying->get_native_handle();
        }
        auto _handle_io_result(int result, bool allow_clean_shutdown) const -> std::size_t
        {
            auto error = ::SSL_get_error(_ssl.get(), result);
            if (result > 0) {
                _state = state::good;
                return up::integral_caster(std::make_unsigned_t<decltype(result)>(result));
            } else if (result == 0 && error == SSL_ERROR_ZERO_RETURN && allow_clean_shutdown) {
                /* Clean ssl shutdown; however, note that the socket might
                 * still be open. */
                _state = state::good;
                return 0;
            } else if (result < 0 && error == SSL_ERROR_WANT_READ) {
                UP_RAISE(unreadable, "unreadable-tls-stream"_s);
            } else if (result < 0 && error == SSL_ERROR_WANT_WRITE) {
                UP_RAISE(unwritable, "unwritable-tls-stream"_s);
            } else {
                _state = state::bad;
                raise_ssl_error("bad-ssl-io-error"_s, result, error);
            }
        }
    };

}


class up_tls::tls::authority::impl
{
public: // --- scope ---
    class system;
    class directory;
    class file;
    class certificate;
private: // --- state ---
    std::shared_ptr<const impl> _parent;
protected: // --- life ---
    explicit impl() = default;
    explicit impl(std::shared_ptr<const impl>&& parent)
        : _parent(std::move(parent))
    { }
    ~impl() noexcept = default;
public: // --- operations ---
    void apply(SSL_CTX* ctx) const
    {
        if (_parent) {
            _parent->apply(ctx);
        }
        _apply(ctx);
    }
private:
    virtual void _apply(SSL_CTX* ctx) const = 0;
};


class up_tls::tls::authority::impl::system final : public impl
{
public: // --- life ---
    /* WORKAROUND: Compilation with Clang and libc++ fails with a default
     * constructor and a non-empty class (e.g. vtable). */
    explicit system(std::nullptr_t dummy __attribute__((unused))) { }
private: // --- operations ---
    void _apply(SSL_CTX* ctx) const override
    {
        if (int rv = ::SSL_CTX_set_default_verify_paths(ctx) != 1) {
            raise_ssl_error("tls-system-authority-error"_s, rv);
        }
    }
};


class up_tls::tls::authority::impl::directory final : public impl
{
private: // --- state ---
    std::string _pathname;
public: // --- life ---
    explicit directory(std::shared_ptr<const impl> parent, std::string&& pathname)
        : impl(std::move(parent)), _pathname(std::move(pathname))
    { }
private: // --- operations ---
    void _apply(SSL_CTX* ctx) const override
    {
        if (int rv = ::SSL_CTX_load_verify_locations(ctx, nullptr, _pathname.c_str()) != 1) {
            raise_ssl_error("tls-directory-authority-error"_s, rv, _pathname);
        }
    }
};


class up_tls::tls::authority::impl::file final : public impl
{
private: // --- state ---
    std::string _pathname;
public: // --- life ---
    explicit file(std::shared_ptr<const impl> parent, std::string&& pathname)
        : impl(std::move(parent)), _pathname(std::move(pathname))
    { }
private: // --- operations ---
    void _apply(SSL_CTX* ctx) const override
    {
        if (int rv = ::SSL_CTX_load_verify_locations(ctx, _pathname.c_str(), nullptr) != 1) {
            raise_ssl_error("tls-file-authority-error"_s, rv, _pathname);
        }
    }
};


class up_tls::tls::authority::impl::certificate final : public impl
{
private: // --- state ---
    x509 _certificate;
public: // --- life ---
    explicit certificate(std::shared_ptr<const impl> parent, const up::buffer& buffer)
        : impl(std::move(parent)), _certificate(buffer)
    { }
private: // --- operations ---
    void _apply(SSL_CTX* ctx) const override
    {
        if (auto store = ::SSL_CTX_get_cert_store(ctx)) {
            if (int rv = ::X509_STORE_add_cert(store, _certificate.native_handle()) != 1) {
                raise_ssl_error("tls-bad-certificate-error"_s, rv);
            } // else: ok
        } else {
            raise_ssl_error("tls-bad-certificate-error"_s);
        }
    }
};


auto up_tls::tls::authority::system() -> authority
{
    return authority(std::make_shared<const impl::system>(nullptr));
}

up_tls::tls::authority::authority(std::shared_ptr<const impl> impl)
    : _impl(std::move(impl))
{ }

auto up_tls::tls::authority::with_directory(std::string pathname) -> authority
{
    return authority(std::make_shared<const impl::directory>(_impl, std::move(pathname)));
}

auto up_tls::tls::authority::with_file(std::string pathname) -> authority
{
    return authority(std::make_shared<const impl::file>(_impl, std::move(pathname)));
}

auto up_tls::tls::authority::with_certificate(const up::buffer& buffer) -> authority
{
    openssl_thread::instance();
    return authority(std::make_shared<const impl::certificate>(_impl, buffer));
}


class up_tls::tls::identity::impl final
{
private: // --- state ---
    std::string _private_key_pathname;
    std::string _certificate_pathname;
    up::optional<std::string> _certificate_chain_pathname;
public: // --- life ---
    explicit impl(
        std::string&& private_key_pathname,
        std::string&& certificate_pathname,
        up::optional<std::string>&& certificate_chain_pathname)
        : _private_key_pathname(std::move(private_key_pathname))
        , _certificate_pathname(std::move(certificate_pathname))
        , _certificate_chain_pathname(std::move(certificate_chain_pathname))
    { }
public: // --- operations ---
    void apply(SSL_CTX* ctx) const
    {
        /* The SSL_CTX also contains functions to add private key and
         * certificate from a memory buffer. However, there is no
         * corresponding function for the certificate chain. */
        if (int rv = ::SSL_CTX_use_PrivateKey_file(ctx, _private_key_pathname.c_str(), SSL_FILETYPE_PEM) != 1) {
            raise_ssl_error("tls-private-key-error"_s, rv);
        }
        if (int rv = ::SSL_CTX_use_certificate_file(ctx, _certificate_pathname.c_str(), SSL_FILETYPE_PEM) != 1) {
            raise_ssl_error("tls-certificate-error"_s, rv);
        }
        if (_certificate_chain_pathname) {
            if (int rv = ::SSL_CTX_use_certificate_chain_file(ctx, _certificate_chain_pathname->c_str()) != 1) {
                raise_ssl_error("tls-certificate-chain-error"_s, rv);
            }
        }
    }
};


up_tls::tls::identity::identity(
    std::string private_key_pathname,
    std::string certificate_pathname)
    : _impl(std::make_shared<const impl>(
            std::move(private_key_pathname),
            std::move(certificate_pathname),
            up::optional<std::string>{}))
{ }

up_tls::tls::identity::identity(
    std::string private_key_pathname,
    std::string certificate_pathname,
    up::optional<std::string> certificate_chain_pathname)
    : _impl(std::make_shared<const impl>(
            std::move(private_key_pathname),
            std::move(certificate_pathname),
            std::move(certificate_chain_pathname)))
{ }


class up_tls::tls::context::impl final
{
public: // --- scope ---
    using self = impl;
    static auto make_ssl_ctx(const SSL_METHOD* method)
    {
        openssl_thread::instance();
        return std::unique_ptr<SSL_CTX, decltype(&::SSL_CTX_free)>{
            ::SSL_CTX_new(method), &::SSL_CTX_free};
    }
public: // --- state ---
    std::unique_ptr<SSL_CTX, decltype(&::SSL_CTX_free)> _ctx;
    std::shared_ptr<const identity::impl> _identity;
public: // --- life ---
    explicit impl(const std::shared_ptr<const authority::impl>& authority, std::shared_ptr<const identity::impl>&& identity)
        : _ctx(make_ssl_ctx(SSLv23_client_method())) // XXX: add support for server mode
        , _identity(std::move(identity))
    {
        if (_ctx == nullptr) {
            raise_ssl_error("bad-ssl-context-error"_s);
        }
        ::SSL_CTX_set_options(_ctx.get(), SSL_OP_NO_SSLv2);
        ::SSL_CTX_set_options(_ctx.get(), SSL_OP_NO_SSLv3);
        ::SSL_CTX_set_options(_ctx.get(), SSL_OP_NO_TLSv1);
        ::SSL_CTX_set_options(_ctx.get(), SSL_OP_NO_TLSv1_1);
        ::SSL_CTX_set_mode(_ctx.get(), SSL_MODE_ENABLE_PARTIAL_WRITE);
        /* OpenSSL DOC (not sure what that really means): Make it possible to
         * retry SSL_write() with changed buffer location (the buffer contents
         * must stay the same). This is not the default to avoid the
         * misconception that non-blocking SSL_write() behaves like
         * non-blocking write(). */
        ::SSL_CTX_set_mode(_ctx.get(), SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
        /* The following option might save memory per
         * connection. Unfortunately, the documentation doesn't mention the
         * drawbacks, so it's unclear if the options is really useful. */
        ::SSL_CTX_set_mode(_ctx.get(), SSL_MODE_RELEASE_BUFFERS);
        if (authority) {
            authority->apply(_ctx.get());
        }
        if (_identity) {
            _identity->apply(_ctx.get());
        }
    }
};


up_tls::tls::context::context(const authority& authority, identity identity)
    : _impl(std::make_shared<const impl>(authority._impl, std::move(identity._impl)))
{ }

auto up_tls::tls::context::make_engine(up::impl_ptr<up::stream::engine> engine, up::stream::await& awaiting)
    -> up::impl_ptr<up::stream::engine>
{
    return up::make_impl<tls_stream>(_impl->_ctx.get(), std::move(engine), awaiting);
}
