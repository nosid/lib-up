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
#include "openssl/x509v3.h"

#include "up_buffer_adapter.hpp"
#include "up_char_cast.hpp"
#include "up_defer.hpp"
#include "up_exception.hpp"
#include "up_ints.hpp"
#include "up_utility.hpp"

namespace
{

    struct runtime { };

    struct already_shutdown { };


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
        __attribute__((unused)) // required because of compile-type type dispatch
        static void _put_thread_id(CRYPTO_THREADID* result, void* value)
        {
            ::CRYPTO_THREADID_set_pointer(result, value);
        }
        template <typename Integer,
                  typename = std::enable_if_t<std::is_unsigned<Integer>::value>>
        __attribute__((unused)) // required because of compile-type type dispatch
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
        int _ssl_ex_data_index = -1;
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
            /* Allocate an index in the SSL* structure, which can be used
             * (solely) by this framework. */
            _ssl_ex_data_index = ::SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
            if (_ssl_ex_data_index < 0) {
                up::raise<runtime>("tls-external-data-error");
            }
            // required for multi-threaded programs
            ::CRYPTO_THREADID_set_callback(&_thread_id_callback);
            /* It is important that the callback is installed last. Otherwise,
             * a deadlock might occur, because the locking callback is invoked
             * and tries to access the singleton, which is still locked as
             * long as it is instantiated. */
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
        template <typename Type>
        void ssl_put_ptr(SSL* ssl, Type* ptr)
        {
            if (::SSL_set_ex_data(ssl, _ssl_ex_data_index, ptr) != 1) {
                up::raise<runtime>("tls-external-data-error");
            }
        }
        void ssl_reset_ptr(SSL* ssl)
        {
            if (::SSL_set_ex_data(ssl, _ssl_ex_data_index, nullptr) != 1) {
                up::raise<runtime>("tls-external-data-error");
            }
        }
        template <typename Type>
        auto ssl_get_ptr(SSL* ssl) -> Type*
        {
            if (auto* ptr = ::SSL_get_ex_data(ssl, _ssl_ex_data_index)) {
                return static_cast<Type*>(ptr);
            } else {
                up::raise<runtime>("tls-external-data-error");
            }
        }
    };


    class openssl_thread final
    {
    public: // --- scope ---
        using self = openssl_thread;
        static auto instance() -> auto&
        {
            static thread_local self instance;
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
    void raise_ssl_error(up::source source, Args&&... args)
    {
        if (auto code = ::ERR_get_error()) {
            /* Throw first error and discard all others silently. */
            auto lib = ::ERR_lib_error_string(code);
            auto func = ::ERR_lib_error_string(code);
            auto reason = ::ERR_reason_error_string(code);
            ::ERR_clear_error();
            up::raise<runtime>(std::move(source), code, lib, func, reason, std::forward<Args>(args)...);
        } else {
            up::raise<runtime>(std::move(source), std::forward<Args>(args)...);
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
                raise_ssl_error("tls-bad-certificate-error");
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
                return up::ints::caster(stream->write_some({data, up::ints::caster(size)}));
            } catch (const up::exception<engine::unreadable>&) {
                ::BIO_set_retry_read(bio);
                return -1;
            } catch (const up::exception<engine::unwritable>&) {
                ::BIO_set_retry_write(bio);
                return -1;
            } catch (...) {
                up::suppress_current_exception("tls-bio-write-callback");
                return -1;
            }
        }
        static int _bread(BIO* bio, char* data, int size)
        {
            using engine = up::stream::engine;
            engine* stream = static_cast<engine*>(bio->ptr);
            try {
                ::BIO_clear_retry_flags(bio);
                return up::ints::caster(stream->read_some({data, up::ints::caster(size)}));
            } catch (const up::exception<engine::unreadable>&) {
                ::BIO_set_retry_read(bio);
                return -1;
            } catch (const up::exception<engine::unwritable>&) {
                ::BIO_set_retry_write(bio);
                return -1;
            } catch (...) {
                up::suppress_current_exception("tls-bio-read-callback");
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
                bio->shutdown = up::ints::caster(num);
                return 1;
            case BIO_CTRL_DUP:
                return 1;
            case BIO_CTRL_FLUSH:
                return 1;
            case BIO_CTRL_PUSH:
            case BIO_CTRL_POP:
                return 0; // ignore (optional, nothing to do)
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
        "tls-bio-adapter", // name
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


    class base_engine : public up::stream::engine
    {
    protected: // --- scope ---
        using self = base_engine;
        using patience = up::stream::patience;
        enum class state { good, bad, read_in_progress, write_in_progress, shutdown_in_progress, shutdown_completed, };
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
            const base_engine* _owner;
            spin_lock _lock;
        public: // --- life ---
            explicit sentry(const base_engine* owner, state expected)
                : _owner(owner), _lock(_owner->_lock)
            {
                if (_owner->_state == state::good) {
                    _owner->_state = expected;
                } else if (_owner->_state == expected) {
                    // nothing
                } else if (_owner->_state == state::shutdown_completed) {
                    up::raise<already_shutdown>("tls-stream-already-shutdown");
                } else {
                    up::raise<runtime>("tls-bad-state",
                        up::to_underlying_type(_owner->_state),
                        up::to_underlying_type(expected));
                }
            }
        };
        using ssl_ptr = std::unique_ptr<SSL, decltype(&::SSL_free)>;
        static auto make_ssl(SSL_CTX* ctx)
        {
            openssl_thread::instance();
            return ssl_ptr(::SSL_new(ctx), &::SSL_free);
        }
    protected: // --- state ---
        ssl_ptr _ssl;
        std::unique_ptr<up::stream::engine> _underlying;
        /* Spin lock: Apparently, OpenSSL is not thread-safe in the same way
         * as POSIX socket. That means, you can't read from and write to the
         * same TLS stream at the same time. The lock is used to protect
         * against this kind of use. A spin lock is used, because the stream
         * should not be used that way, so it causes basically no overhead. */
        mutable std::atomic_flag _lock = ATOMIC_FLAG_INIT;
        mutable state _state = state::bad;
    protected: // --- life ---
        explicit base_engine(
            ssl_ptr ssl, std::unique_ptr<up::stream::engine> underlying, patience& patience, int handshake(SSL*))
            : _ssl(std::move(ssl)), _underlying(std::move(underlying))
        {
            if (_ssl == nullptr) {
                raise_ssl_error("tls-ssl-error");
            }
            /* User-defined BIO. Note that if the BIO is associated with SSL,
             * it is automatically freed in SSL_free. */
            BIO* bio = ::BIO_new(&bio_adapter::methods);
            bio->ptr = _underlying.get();
            bio->init = 1;
            ::SSL_set_bio(_ssl.get(), bio, bio);
            for (;;) {
                auto result = handshake(_ssl.get());
                if (result == 1) {
                    /* OK. The TLS/SSL handshake was successfully completed, a
                     * TLS/SSL connection has been established. */
                    break;
                } else if (result == 0) {
                    /* The TLS/SSL handshake was not successful but was shut
                     * down controlled and by the specifications of the
                     * TLS/SSL protocol. */
                    raise_ssl_error("tls-handshake-error",
                        result, ::SSL_get_error(_ssl.get(), result));
                } else {
                    auto error = ::SSL_get_error(_ssl.get(), result);
                    if (error == SSL_ERROR_WANT_READ) {
                        patience(_underlying->get_native_handle(), patience::operation::read);
                    } else if (error == SSL_ERROR_WANT_WRITE) {
                        patience(_underlying->get_native_handle(), patience::operation::write);
                    } else {
                        raise_ssl_error("tls-io-error", result, error);
                    }
                }
            }
            _state = state::good;
        }
        ~base_engine() noexcept = default;
    private: // --- operations ---
        void shutdown() const override final
        {
            try {
                sentry sentry(this, state::shutdown_in_progress);
                _graceful_shutdown();
            } catch (const up::exception<already_shutdown>&) {
                /* Nothing, i.e. simulate the behavior of a regular socket
                 * that has been uni-directionally shutdown. */
            }
            _underlying->shutdown();
        }
        void hard_close() const override final
        {
            _state = state::bad;
            _underlying->hard_close();
        }
        auto read_some(up::chunk::into chunk) const -> std::size_t override final
        {
            try {
                sentry sentry(this, state::read_in_progress);
                openssl_thread::instance();
                return _handle_io_result(
                    ::SSL_read(_ssl.get(), chunk.data(), up::ints::caster(chunk.size())), true);
            } catch (const up::exception<already_shutdown>&) {
                /* Simulate behavior of a regular socket that has been
                 * uni-directionally shutdown. */
                return 0;
            }
        }
        auto write_some(up::chunk::from chunk) const -> std::size_t override final
        {
            sentry sentry(this, state::write_in_progress);
            openssl_thread::instance();
            return _handle_io_result(
                ::SSL_write(_ssl.get(), chunk.data(), up::ints::caster(chunk.size())), false);
        }
        auto read_some_bulk(up::chunk::into_bulk_t& chunks) const -> std::size_t override final
        {
            /* Unfortunately, OpenSSL has no support for multiple buffers. So,
             * we only process the first non-empty buffer. */
            return read_some(chunks.head());
        }
        auto write_some_bulk(up::chunk::from_bulk_t& chunks) const -> std::size_t override final
        {
            /* Unfortunately, OpenSSL has no support for multiple buffers. So,
             * we only process the first non-empty buffer. */
            return write_some(chunks.head());
        }
        auto downgrade() -> std::unique_ptr<up::stream::engine> override final
        {
            sentry sentry(this, state::shutdown_in_progress);
            _graceful_shutdown();
            return std::move(_underlying);
        }
        auto get_underlying_engine() const -> const engine* override final
        {
            return _underlying->get_underlying_engine();
        }
        auto get_native_handle() const -> up::stream::native_handle override final
        {
            return _underlying->get_native_handle();
        }
        void _graceful_shutdown() const
        {
            openssl_thread::instance();
            for (;;) {
                int result = ::SSL_shutdown(_ssl.get());
                if (result == 1) {
                    // The shutdown was successfully completed.
                    break;
                } else if (result == 0) {
                    /* restart: SSL_shutdown must be called again to complete
                     * the bidirectional shutdown handshake. */
                } else if (result < 0) {
                    auto error = ::SSL_get_error(_ssl.get(), result);
                    if (error == SSL_ERROR_WANT_READ) {
                        up::raise<unreadable>("unreadable-tls-stream");
                    } else if (error == SSL_ERROR_WANT_WRITE) {
                        up::raise<unwritable>("unwritable-tls-stream");
                    } else if (error == SSL_ERROR_SYSCALL && errno == 0) {
                        /* According to the OpenSSL documentation, an
                         * erroneous SSL_ERROR_SYSCALL may be flagged even
                         * though no error occurred. */
                        break;
                    } else {
                        _state = state::bad;
                        raise_ssl_error("tls-io-error", result, error);
                    }
                } else {
                    _state = state::bad;
                    raise_ssl_error("tls-io-error", result);
                }
            }
            _state = state::shutdown_completed;
        }
        auto _handle_io_result(int result, bool allow_shutdown) const -> std::size_t
        {
            auto error = ::SSL_get_error(_ssl.get(), result);
            if (result > 0) {
                _state = state::good;
                return up::ints::caster(std::make_unsigned_t<decltype(result)>(result));
            } else if (allow_shutdown && result == 0 && error == SSL_ERROR_ZERO_RETURN) {
                /* Clean ssl shutdown; however, note that the socket might
                 * still be open. */
                _state = state::shutdown_completed;
                return 0;
            } else if (allow_shutdown && result == 0 && error == SSL_ERROR_SYSCALL && errno == 0) {
                /* Apparently this case happens when the other side closes the
                 * connection without a correct shutdown. Because it happens
                 * with many servers, this issue is silently ignored. */
                _state = state::shutdown_completed;
                return 0;
            } else if (result < 0 && error == SSL_ERROR_WANT_READ) {
                up::raise<unreadable>("unreadable-tls-stream");
            } else if (result < 0 && error == SSL_ERROR_WANT_WRITE) {
                up::raise<unwritable>("unwritable-tls-stream");
            } else {
                _state = state::bad;
                raise_ssl_error("tls-io-error", result, error);
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
    void apply(SSL_CTX* ctx, STACK_OF(X509_NAME)* names) const
    {
        if (_parent) {
            _parent->apply(ctx, names);
        }
        _apply(ctx, names);
    }
private:
    virtual void _apply(SSL_CTX* ctx, STACK_OF(X509_NAME)* names) const = 0;
protected:
    bool _load_directory(SSL_CTX* ctx, STACK_OF(X509_NAME)* names, const char* pathname) const
    {
        int rv = ::SSL_CTX_load_verify_locations(ctx, nullptr, pathname);
        if (rv != 1) {
            return rv;
        } else if (names == nullptr) {
            return rv; // nothing else to do
        } else if ((rv = ::SSL_add_dir_cert_subjects_to_stack(names, pathname)) == 1) {
            return rv;
        } else {
            raise_ssl_error("tls-internal-authority-error", rv, std::string(pathname));
        }
    }
    bool _load_file(SSL_CTX* ctx, STACK_OF(X509_NAME)* names, const char* pathname) const
    {
        int rv = ::SSL_CTX_load_verify_locations(ctx, pathname, nullptr);
        if (rv != 1) {
            return rv;
        } else if (names == nullptr) {
            return rv; // nothing else to do
        } else if ((rv = ::SSL_add_file_cert_subjects_to_stack(names, pathname)) == 1) {
            return rv;
        } else {
            raise_ssl_error("tls-internal-authority-error", rv, std::string(pathname));
        }
    }
};


class up_tls::tls::authority::impl::system final : public impl
{
public: // --- life ---
    explicit system() = default;
private: // --- operations ---
    void _apply(SSL_CTX* ctx, STACK_OF(X509_NAME)* names) const override
    {
        auto file = _lookup(::X509_get_default_cert_file_env(), ::X509_get_default_cert_file());
        auto directory = _lookup(::X509_get_default_cert_dir_env(), ::X509_get_default_cert_dir());
        int rv = _load_file(ctx, names, file);
        int rw = _load_directory(ctx, names, directory);
        /* This function mimics the behavior of
         * SSL_CTX_set_default_verify_paths, i.e. it only fails if both file
         * and directory lookup fail. */
        if (rv != 1 && rw != 1) {
            raise_ssl_error("tls-system-authority-error",
                std::string(file), rv, std::string(directory), rw);
        }
    }
    auto _lookup(const char* name, const char* fallback) const -> const char*
    {
        const char* result = std::getenv(name);
        return result ? result : fallback;
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
    void _apply(SSL_CTX* ctx, STACK_OF(X509_NAME)* names) const override
    {
        int rv = _load_directory(ctx, names, _pathname.c_str());
        if (rv != 1) {
            raise_ssl_error("tls-directory-authority-error", rv, _pathname);
        } // else: OK
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
    void _apply(SSL_CTX* ctx, STACK_OF(X509_NAME)* names) const override
    {
        int rv = _load_file(ctx, names, _pathname.c_str());
        if (rv != 1) {
            raise_ssl_error("tls-file-authority-error", rv, _pathname);
        } // else: OK
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
    void _apply(SSL_CTX* ctx, STACK_OF(X509_NAME)* names) const override
    {
        if (auto store = ::SSL_CTX_get_cert_store(ctx)) {
            X509* x509 = _certificate.native_handle();
            int rv = ::X509_STORE_add_cert(store, x509);
            if (rv != 1) {
                raise_ssl_error("tls-bad-certificate-error", rv);
            } else if (names == nullptr) {
                // nothing else to do
            } else if (X509_NAME* name = ::X509_get_subject_name(x509)) {
                if (X509_NAME* dup = ::X509_NAME_dup(name)) {
                    if (sk_X509_NAME_find(names, dup) < 0) {
                        sk_X509_NAME_push(names, dup);
                    } else {
                        ::X509_NAME_free(dup);
                    }
                } else {
                    raise_ssl_error("tls-bad-certificate-error");
                }
            } else {
                raise_ssl_error("tls-bad-certificate-error");
            }
        } else {
            raise_ssl_error("tls-bad-certificate-error");
        }
    }
};


auto up_tls::tls::authority::system() -> authority
{
    return authority(std::make_shared<const impl::system>());
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
        int rv;
        rv = ::SSL_CTX_use_PrivateKey_file(ctx, _private_key_pathname.c_str(), SSL_FILETYPE_PEM);
        if (rv != 1) {
            raise_ssl_error("tls-private-key-error", rv);
        }
        rv = ::SSL_CTX_use_certificate_file(ctx, _certificate_pathname.c_str(), SSL_FILETYPE_PEM);
        if (rv != 1) {
            raise_ssl_error("tls-certificate-error", rv);
        }
        if (_certificate_chain_pathname) {
            rv = ::SSL_CTX_use_certificate_chain_file(ctx, _certificate_chain_pathname->c_str());
            if (rv != 1) {
                raise_ssl_error("tls-certificate-chain-error", rv);
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
            up::optional<std::string>()))
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


class up_tls::tls::certificate::impl final
{
public: // --- state ---
    X509* _x509;
public: // --- life ---
    explicit impl(X509* x509)
        : _x509(x509)
    { }
};


auto up_tls::tls::certificate::common_name() const -> up::optional<std::string>
{
    X509_NAME* subject = ::X509_get_subject_name(_impl._x509);
    int pos = -1;
    X509_NAME_ENTRY* entry = nullptr;
    while ((pos = ::X509_NAME_get_index_by_NID(subject, NID_commonName, pos)) != -1) {
        entry = ::X509_NAME_get_entry(subject, pos);
    }
    if (entry) {
        unsigned char* buffer;
        int rv = ::ASN1_STRING_to_UTF8(&buffer, ::X509_NAME_ENTRY_get_data(entry));
        if (rv >= 0) {
            UP_DEFER { ::OPENSSL_free(buffer); };
            return std::string(up::char_cast<char>(buffer), up::ints::cast<std::size_t>(rv));
        } else {
            raise_ssl_error("tls-bad-common-name");
        }
    } else {
        return up::nullopt;
    }
}

bool up_tls::tls::certificate::matches_hostname(up::string_view hostname) const
{
    int rv = ::X509_check_host(_impl._x509, hostname.data(), hostname.size(), 0, nullptr);
    if (rv == 1) {
        return true;
    } else if (rv == 0) {
        return false;
    } else {
        raise_ssl_error("tls-bad-hostname-check", hostname);
    }
}


class up_tls::tls::context
{
protected: // --- scope ---
    using self = context;
    using ssl_ctx_ptr = std::unique_ptr<SSL_CTX, decltype(&::SSL_CTX_free)>;
    using authority_ptr = std::shared_ptr<const up_tls::tls::authority::impl>;
    using identity_ptr = std::shared_ptr<const up_tls::tls::identity::impl>;
    static auto make_ssl_ctx(const SSL_METHOD* method)
    {
        openssl_thread::instance();
        return ssl_ctx_ptr(::SSL_CTX_new(method), &::SSL_CTX_free);
    }
protected: // --- state ---
    ssl_ctx_ptr _ssl_ctx;
    authority_ptr _authority;
    identity_ptr _identity;
protected: // --- life ---
    explicit context(
        ssl_ctx_ptr&& ssl_ctx,
        up::optional<authority>&& authority,
        up::optional<identity>&& identity)
        : _ssl_ctx(std::move(ssl_ctx))
        , _authority(authority ? std::move(authority->_impl) : authority_ptr())
        , _identity(identity ? std::move(identity->_impl) : identity_ptr())
    {
        if (_ssl_ctx == nullptr) {
            raise_ssl_error("tls-bad-context-error");
        }
        /* Disable compression, because there have been some security issues
         * with compression in the past, and apparently compression will be
         * removed from TLSv1.3 at all.*/
        ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_COMPRESSION);
        /* Disable some obsolete protocol versions completely. */
        ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_SSLv2);
        ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_SSLv3);
        /* Recommended security improvements. */
        ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_SINGLE_DH_USE);
        ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_SINGLE_ECDH_USE);
        /* Disable support for stateless session resumption with RFC4507bis
         * tickets, because clients should use connections efficiently instead
         * of optimizing connection setup. */
        ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_TICKET);
        /* The default maximum depth is 100. This value seems to be way to
         * high for real use cases. Be a bit more restrictive. */
        ::SSL_CTX_set_verify_depth(_ssl_ctx.get(), 7);
        /* Completely disable session caching, because clients should use
         * connections efficiently instead of optimizing connection setup. */
        ::SSL_CTX_set_session_cache_mode(_ssl_ctx.get(), SSL_SESS_CACHE_OFF);
        /* Allow SSL_write to return when only some data has been written,
         * similar to the default behavior of write(2). */
        ::SSL_CTX_set_mode(_ssl_ctx.get(), SSL_MODE_ENABLE_PARTIAL_WRITE);
        /* OpenSSL DOC (not sure what that really means): Make it possible to
         * retry SSL_write() with changed buffer location (the buffer contents
         * must stay the same). This is not the default to avoid the
         * misconception that non-blocking SSL_write() behaves like
         * non-blocking write(). */
        ::SSL_CTX_set_mode(_ssl_ctx.get(), SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
        /* The following option might save memory per connection.
         * Unfortunately, the documentation doesn't mention the drawbacks, so
         * it's unclear if the options is really useful. */
        ::SSL_CTX_set_mode(_ssl_ctx.get(), SSL_MODE_RELEASE_BUFFERS);
    }
    ~context() noexcept = default;
public: // --- operations ---
    auto get_underlying_ssl_ctx() -> SSL_CTX*
    {
        return _ssl_ctx.get();
    }
};


class up_tls::tls::server_context::impl final : public context
{
public: // --- scope ---
    class server_engine;
    class auxiliary
    {
    public: // --- state ---
        const hostname_callback& _callback;
    protected: // --- life ---
        explicit auxiliary(const hostname_callback& callback)
            : _callback(callback)
        { }
        ~auxiliary() noexcept = default;
    };
private:
    static int _hostname_callback(SSL* ssl, int*, void*)
    {
        auto&& callback = openssl_process::instance().ssl_get_ptr<auxiliary>(ssl)->_callback;
        if (auto servername = ::SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name)) {
            try {
                auto&& other = callback(std::string(servername));
                ::SSL_set_SSL_CTX(ssl, other._impl->_ssl_ctx.get());
                return SSL_TLSEXT_ERR_OK;
            } catch (const up::exception<accept_hostname>&) {
                return SSL_TLSEXT_ERR_OK;
            } catch (const up::exception<reject_hostname>&) {
                return SSL_TLSEXT_ERR_NOACK;
            } catch (...) {
                up::suppress_current_exception("tls-hostname-callback");
                return SSL_TLSEXT_ERR_ALERT_FATAL;
            }
        } else {
            /* In this case the client didn't send a hostname. The handshake
             * will be continued with the current SSL_CTX. */
            return SSL_TLSEXT_ERR_OK;
        }
    }
public: // --- life ---
    explicit impl(identity&& identity, options&& options)
        : context(make_ssl_ctx(::SSLv23_server_method()), up::optional<authority>(), std::move(identity))
    {
        if (options.none(option::tls_v10)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_TLSv1);
        }
        if (options.none(option::tls_v11)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_TLSv1_1);
        }
        if (options.none(option::tls_v12) && options.any(option::tls_v10, option::tls_v11)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_TLSv1_2);
        }
        if (options.all(option::workarounds)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_ALL);
        }
        if (options.all(option::cipher_server_preference)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_CIPHER_SERVER_PREFERENCE);
        }
        /* Reduce the possibilities to resume sessions. */
        ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
        ::SSL_CTX_set_verify(_ssl_ctx.get(), SSL_VERIFY_NONE, nullptr);
        _identity->apply(_ssl_ctx.get());
        ::SSL_CTX_set_tlsext_servername_callback(_ssl_ctx.get(), &_hostname_callback);
    }
};


class up_tls::tls::server_context::impl::server_engine final : private auxiliary, public base_engine
{
private: // --- scope ---
    static auto prepare(SSL_CTX* ssl_ctx, auxiliary* auxiliary) -> ssl_ptr
    {
        ssl_ptr ssl = make_ssl(ssl_ctx);
        openssl_process::instance().ssl_put_ptr(ssl.get(), auxiliary);
        return ssl;
    }
public: // --- life ---
    explicit server_engine(
        SSL_CTX* ssl_ctx,
        std::unique_ptr<up::stream::engine> underlying,
        patience& patience,
        const hostname_callback& callback)
        : auxiliary(callback)
        , base_engine(prepare(ssl_ctx, this), std::move(underlying), patience, ::SSL_accept)
    {
        openssl_process::instance().ssl_reset_ptr(_ssl.get());
    }
};


void up_tls::tls::server_context::destroy(impl* ptr)
{
    std::default_delete<impl>()(ptr);
}

auto up_tls::tls::server_context::ignore_hostname() -> hostname_callback
{
    return [](std::string hostname) -> self& {
        up::raise<accept_hostname>("tls-ignore-hostname", std::move(hostname));
    };
}


up_tls::tls::server_context::server_context(identity identity, options options)
    : _impl(up::impl_make(std::move(identity), std::move(options)))
{ }

auto up_tls::tls::server_context::upgrade(
    std::unique_ptr<up::stream::engine> engine,
    up::stream::patience& patience,
    const hostname_callback& callback)
    -> std::unique_ptr<up::stream::engine>
{
    return std::make_unique<impl::server_engine>(
        _impl->get_underlying_ssl_ctx(), std::move(engine), patience, callback);
}


class up_tls::tls::secure_context::impl final : public context
{
public: // --- scope ---
    class secure_engine;
    class auxiliary
    {
    public: // --- state ---
        const verify_callback& _callback;
    protected: // --- life ---
        explicit auxiliary(const verify_callback& callback)
            : _callback(callback)
        { }
        ~auxiliary() noexcept = default;
    };
private:
    static int _verify_callback(int preverified, X509_STORE_CTX* x509_store)
    {
        auto index = ::SSL_get_ex_data_X509_STORE_CTX_idx();
        if (index < 0) {
            raise_ssl_error("tls-internal-certificate-error");
        }
        if (SSL* ssl = static_cast<SSL*>(::X509_STORE_CTX_get_ex_data(x509_store, index))) {
            auto&& callback = openssl_process::instance().ssl_get_ptr<auxiliary>(ssl)->_callback;
            auto depth = up::ints::cast<std::size_t>(::X509_STORE_CTX_get_error_depth(x509_store));
            X509* x509 = ::X509_STORE_CTX_get_current_cert(x509_store);
            try {
                certificate::impl impl(x509);
                int result = callback(preverified == 1, depth, certificate(impl));
                if (result == 1 && preverified != 1) {
                    ::X509_STORE_CTX_set_error(x509_store, X509_V_OK); // clear error
                } else if (result != 1 && preverified == 1) {
                    ::X509_STORE_CTX_set_error(x509_store, X509_V_ERR_APPLICATION_VERIFICATION);
                } // else: nothing
                return result;
            } catch (...) {
                up::suppress_current_exception("tls-verify-callback");
                ::X509_STORE_CTX_set_error(x509_store, X509_V_ERR_APPLICATION_VERIFICATION);
                return 0;
            }
        } else {
            raise_ssl_error("tls-internal-certificate-error");
        }
    }
public: // --- life ---
    explicit impl(authority&& authority, identity&& identity, options&& options)
        : context(make_ssl_ctx(::SSLv23_server_method()), std::move(authority), std::move(identity))
    {
        if (options.none(option::tls_v10)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_TLSv1);
        }
        if (options.none(option::tls_v11)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_TLSv1_1);
        }
        if (options.none(option::tls_v12) && options.any(option::tls_v10, option::tls_v11)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_TLSv1_2);
        }
        if (options.all(option::workarounds)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_ALL);
        }
        if (options.all(option::cipher_server_preference)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_CIPHER_SERVER_PREFERENCE);
        }
        /* Reduce the possibilities to resume sessions. */
        ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
        /* A peer certificate is requested and verified in all cases, even if
         * the authority is empty. The verify_callback should be used for
         * additional checks and can be used to override the default
         * verification result. */
        /* The server must provide a list of expected CAs to the client. This
         * implementation uses all certificates from the authority. */
        auto x509_name_cmp = [](const X509_NAME* const* a, const X509_NAME* const* b) {
            return ::X509_NAME_cmp(*a, *b);
        };
        if (STACK_OF(X509_NAME)* names = sk_X509_NAME_new(x509_name_cmp)) {
            UP_DEFER_NAMED(guard) {
                sk_X509_NAME_pop_free(names, ::X509_NAME_free);
            };
            _authority->apply(_ssl_ctx.get(), names);
            ::SSL_CTX_set_client_CA_list(_ssl_ctx.get(), names);
            guard.disarm(); // ownership transferred to _ssl_ctx
            ::SSL_CTX_set_verify(_ssl_ctx.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT | SSL_VERIFY_CLIENT_ONCE, &_verify_callback);
        } else {
            raise_ssl_error("tls-runtime-error");
        }
        _identity->apply(_ssl_ctx.get());
    }
};


class up_tls::tls::secure_context::impl::secure_engine final : private auxiliary, public base_engine
{
private: // --- scope ---
    static auto prepare(SSL_CTX* ssl_ctx, auxiliary* auxiliary) -> ssl_ptr
    {
        ssl_ptr ssl = make_ssl(ssl_ctx);
        openssl_process::instance().ssl_put_ptr(ssl.get(), auxiliary);
        return ssl;
    }
public: // --- life ---
    explicit secure_engine(
        SSL_CTX* ssl_ctx,
        std::unique_ptr<up::stream::engine> underlying,
        patience& patience,
        const verify_callback& callback)
        : auxiliary(callback)
        , base_engine(prepare(ssl_ctx, this), std::move(underlying), patience, ::SSL_accept)
    {
        openssl_process::instance().ssl_reset_ptr(_ssl.get());
        // sanity checks; should have already been done by OpenSSL library
        if (X509* x509 = ::SSL_get_peer_certificate(_ssl.get())) {
            ::X509_free(x509);
        } else {
            up::raise<runtime>("tls-missing-peer-certificate");
        }
        if (::SSL_get_verify_result(_ssl.get()) != X509_V_OK) {
            up::raise<runtime>("tls-invalid-peer-certificate");
        }
    }
};


void up_tls::tls::secure_context::destroy(impl* ptr)
{
    std::default_delete<impl>()(ptr);
}

up_tls::tls::secure_context::secure_context(authority authority, identity identity, options options)
    : _impl(up::impl_make(std::move(authority), std::move(identity), std::move(options)))
{ }

auto up_tls::tls::secure_context::upgrade(
    std::unique_ptr<up::stream::engine> engine,
    up::stream::patience& patience,
    const verify_callback& callback)
    -> std::unique_ptr<up::stream::engine>
{
    return std::make_unique<impl::secure_engine>(
        _impl->get_underlying_ssl_ctx(), std::move(engine), patience, callback);
}


class up_tls::tls::client_context::impl final : public context
{
public: // --- scope ---
    class client_engine;
    class auxiliary
    {
    public: // --- state ---
        const verify_callback& _callback;
    protected: // --- life ---
        explicit auxiliary(const verify_callback& callback)
            : _callback(callback)
        { }
        ~auxiliary() noexcept = default;
    };
private:
    static int _verify_callback(int preverified, X509_STORE_CTX* x509_store)
    {
        auto index = ::SSL_get_ex_data_X509_STORE_CTX_idx();
        if (index < 0) {
            raise_ssl_error("tls-internal-certificate-error");
        }
        if (SSL* ssl = static_cast<SSL*>(::X509_STORE_CTX_get_ex_data(x509_store, index))) {
            auto&& callback = openssl_process::instance().ssl_get_ptr<auxiliary>(ssl)->_callback;
            auto depth = up::ints::cast<std::size_t>(::X509_STORE_CTX_get_error_depth(x509_store));
            X509* x509 = ::X509_STORE_CTX_get_current_cert(x509_store);
            try {
                certificate::impl impl(x509);
                int result = callback(preverified == 1, depth, certificate(impl));
                if (result == 1 && preverified != 1) {
                    ::X509_STORE_CTX_set_error(x509_store, X509_V_OK); // clear error
                } else if (result != 1 && preverified == 1) {
                    ::X509_STORE_CTX_set_error(x509_store, X509_V_ERR_APPLICATION_VERIFICATION);
                } // else: nothing
                return result;
            } catch (...) {
                up::suppress_current_exception("tls-verify-callback");
                ::X509_STORE_CTX_set_error(x509_store, X509_V_ERR_APPLICATION_VERIFICATION);
                return 0;
            }
        } else {
            raise_ssl_error("tls-internal-certificate-error");
        }
    }
public: // --- life ---
    explicit impl(authority&& authority, up::optional<identity>&& identity, options&& options)
        : context(make_ssl_ctx(::SSLv23_client_method()), std::move(authority), std::move(identity))
    {
        if (options.none(option::tls_v10)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_TLSv1);
        }
        if (options.none(option::tls_v11)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_TLSv1_1);
        }
        if (options.none(option::tls_v12) && options.any(option::tls_v10, option::tls_v11)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_NO_TLSv1_2);
        }
        if (options.all(option::workarounds)) {
            ::SSL_CTX_set_options(_ssl_ctx.get(), SSL_OP_ALL);
        }
        _authority->apply(_ssl_ctx.get(), nullptr);
        ::SSL_CTX_set_verify(_ssl_ctx.get(), SSL_VERIFY_PEER, &_verify_callback);
        if (_identity) {
            _identity->apply(_ssl_ctx.get());
        }
    }
};


class up_tls::tls::client_context::impl::client_engine final : private auxiliary, public base_engine
{
private: // --- scope ---
    static auto prepare(
        SSL_CTX* ssl_ctx,
        const up::optional<std::string>& hostname,
        auxiliary* auxiliary) -> ssl_ptr
    {
        ssl_ptr ssl = make_ssl(ssl_ctx);
        if (hostname && !SSL_set_tlsext_host_name(ssl.get(), hostname->c_str())) {
            raise_ssl_error("tls-hostname-error", *hostname);
        }
        openssl_process::instance().ssl_put_ptr(ssl.get(), auxiliary);
        return ssl;
    }
public: // --- life ---
    explicit client_engine(
        SSL_CTX* ssl_ctx,
        std::unique_ptr<up::stream::engine> underlying,
        patience& patience,
        const up::optional<std::string>& hostname,
        const verify_callback& callback)
        : auxiliary(callback)
        , base_engine(prepare(ssl_ctx, hostname, this), std::move(underlying), patience, ::SSL_connect)
    {
        openssl_process::instance().ssl_reset_ptr(_ssl.get());
        // sanity checks; should have already been done by OpenSSL library
        if (X509* x509 = ::SSL_get_peer_certificate(_ssl.get())) {
            ::X509_free(x509);
        } else {
            up::raise<runtime>("tls-missing-peer-certificate");
        }
        if (::SSL_get_verify_result(_ssl.get()) != X509_V_OK) {
            up::raise<runtime>("tls-invalid-peer-certificate");
        }
    }
};


void up_tls::tls::client_context::destroy(impl* ptr)
{
    std::default_delete<impl>()(ptr);
}

up_tls::tls::client_context::client_context(
    authority authority, up::optional<identity> identity, options options)
    : _impl(up::impl_make(std::move(authority), std::move(identity), std::move(options)))
{ }

auto up_tls::tls::client_context::upgrade(
    std::unique_ptr<up::stream::engine> engine,
    up::stream::patience& patience,
    const up::optional<std::string>& hostname,
    const verify_callback& callback)
    -> std::unique_ptr<up::stream::engine>
{
    return std::make_unique<impl::client_engine>(
        _impl->get_underlying_ssl_ctx(), std::move(engine), patience, hostname, callback);
}
