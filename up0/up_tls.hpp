#pragma once

#include "up_buffer.hpp"
#include "up_optional.hpp"
#include "up_stream.hpp"
#include "up_string_view.hpp"
#include "up_swap.hpp"


namespace up_tls
{

    class tls
    {
    public: // --- scope ---
        class authority;
        class identity;
        class certificate;
        class context;
        class server_context;
        class secure_context;
        class client_context;
    };


    /**
     * The class represents a set of trusted root certificates (i.e. usually
     * the certificates from certificate authorities).
     */
    class tls::authority final
    {
    public: // --- scope ---
        using self = authority;
        class impl;
        /* Returns an authority with the default system root certificates. */
        static auto system() -> authority;
        friend context;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        // empty authority
        explicit authority() noexcept = default;
    private:
        explicit authority(std::shared_ptr<const impl> impl);
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        /**
         * From the OpenSSL documentation: The directory to use for server
         * certificate verification. This directory must be in "hash format",
         * see verify for more information. These are also used when building
         * the client certificate chain.
         */
        auto with_directory(up::shared_string pathname) -> authority;
        /**
         * From the OpenSSL documentation: A file containing trusted
         * certificates to use during server authentication and to use when
         * attempting to build the client certificate chain.
         */
        auto with_file(up::shared_string pathname) -> authority;
        /**
         * Add single certificate from the given buffer.
         */
        auto with_certificate(const up::buffer& buffer) -> authority;
    };


    /**
     * This class represents the certificate that is used to cryptographically
     * identify itself towards the peer. It is mandatory for TLS servers and
     * optional for TLS clients.
     */
    class tls::identity final
    {
    public: // --- scope ---
        using self = identity;
        class impl;
        friend context;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit identity(
            up::shared_string private_key_pathname,
            up::shared_string certificate_pathname);
        explicit identity(
            up::shared_string private_key_pathname,
            up::shared_string certificate_pathname,
            up::optional<up::shared_string> certificate_chain_pathname);
    };


    /**
     * The class provides access to the certificate during the verification
     * process.
     */
    class tls::certificate final
    {
    public: // --- scope ---
        using self = certificate;
        class impl;
    private: // --- state ---
        impl& _impl;
    public: // --- life ---
        explicit certificate(impl& impl)
            : _impl(impl)
        { }
    public: // --- operations ---
        // returns most specific common name (if any)
        auto common_name() const -> up::optional<up::unique_string>;
        bool matches_hostname(up::string_view hostname) const;
    };


    /**
     * The server_context supports server name indication (SNI), but does not
     * support client certificates. The latter is supported by the
     * secure_context. The rationale for not supporting both is that it might
     * be too complex for the application developer to use both correct at the
     * same time.
     */
    class tls::server_context final
    {
    public: // --- scope ---
        using self = server_context;
        class impl;
        enum class option : uint8_t { tls_v10, tls_v11, tls_v12, workarounds, cipher_server_preference, };
        using options = up::enum_set<option>;
        using hostname_callback = std::function<self&(up::shared_string)>;
        class accept_hostname { };
        class reject_hostname { };
        static void destroy(impl* ptr);
        static auto ignore_hostname() -> hostname_callback;
    private: // --- state ---
        up::impl_ptr<impl, destroy> _impl;
    public: // --- life ---
        explicit server_context(identity identity, options options);
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        /**
         * The hostname_callback is used for server name indication (SNI).
         */
        auto upgrade(
            std::unique_ptr<up::stream::engine> engine,
            up::stream::patience& patience,
            const hostname_callback& callback)
            -> std::unique_ptr<up::stream::engine>;
    };


    /**
     * The secure_context requests and verifies client certificates, but does
     * not support server name indication (SNI). The latter is supported by
     * the server_context. The rationale for not supporting both is that it
     * might be too complex for the application developer to use both correct
     * at the same time.
     */
    class tls::secure_context final
    {
    public: // --- scope ---
        using self = secure_context;
        class impl;
        enum class option : uint8_t { tls_v10, tls_v11, tls_v12, workarounds, cipher_server_preference, };
        using options = up::enum_set<option>;
        using verify_callback = std::function<bool(bool, std::size_t, const certificate&)>;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, destroy> _impl;
    public: // --- life ---
        /**
         * The authority is used to verify client certificates. A client
         * certificate is requested and verified in all cases, even if the
         * authority is empty.
         */
        explicit secure_context(authority authority, identity identity, options options);
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        /**
         * The verify_callback is invoked for each certificate in the
         * certificate chain provided by the client.
         */
        auto upgrade(
            std::unique_ptr<up::stream::engine> engine,
            up::stream::patience& patience,
            const verify_callback& callback)
            -> std::unique_ptr<up::stream::engine>;
    };


    class tls::client_context final
    {
    public: // --- scope ---
        using self = client_context;
        class impl;
        enum class option : uint8_t { tls_v10, tls_v11, tls_v12, workarounds, };
        using options = up::enum_set<option>;
        using verify_callback = std::function<bool(bool, std::size_t, const certificate&)>;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, destroy> _impl;
    public: // --- life ---
        /**
         * The authority is used for certificate verification. The
         * verify_callback can be used to override the result and accept
         * unknown certificates.
         *
         * If an identity is given it is sent to the server on request.
         */
        explicit client_context(
            authority authority, up::optional<identity> identity, options options);
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        /**
         * The server must provide a certificate. Otherwise the handshake will
         * be aborted. The verify_callback is invoked for each certificate in
         * the certificate chain provided by the server. The verify_callback
         * should be used to perform additional checks (e.g. RFC-2818 hostname
         * verification), and can also be used to accept otherwise rejected
         * certificates (e.g. for DNS-based authentication of named entities).
         */
        auto upgrade(
            std::unique_ptr<up::stream::engine> engine,
            up::stream::patience& patience,
            const up::optional<up::shared_string>& hostname,
            const verify_callback& callback)
            -> std::unique_ptr<up::stream::engine>;
    };

}

namespace up
{

    using up_tls::tls;

}
