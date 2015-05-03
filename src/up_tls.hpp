#pragma once

#include "up_buffer.hpp"
#include "up_optional.hpp"
#include "up_stream.hpp"
#include "up_swap.hpp"


namespace up_tls
{

    class tls
    {
    public: // --- scope ---
        class authority;
        class identity;
        class context;
    };


    class tls::authority final
    {
    public: // --- scope ---
        using self = authority;
        class impl;
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
        auto with_directory(std::string pathname) -> authority;
        /**
         * From the OpenSSL documentation: A file containing trusted
         * certificates to use during server authentication and to use when
         * attempting to build the client certificate chain.
         */
        auto with_file(std::string pathname) -> authority;
        /**
         * Add single certificate from the given buffer.
         */
        auto with_certificate(const up::buffer& buffer) -> authority;
    };


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
            std::string private_key_pathname,
            std::string certificate_pathname);
        explicit identity(
            std::string private_key_pathname,
            std::string certificate_pathname,
            up::optional<std::string> certificate_chain_pathname);
    };


    class tls::context final
    {
    public: // --- scope ---
        using self = context;
        class impl;
    private: // --- state ---
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        explicit context(const authority& authority, identity identity /* XXX: optional? only in Client Mode? */);
        context(const self& rhs) = delete;
        context(self&& rhs) noexcept = default;
        ~context() noexcept = default;
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
        auto make_engine(up::impl_ptr<up::stream::engine> engine, up::stream::await& awaiting)
            -> up::impl_ptr<up::stream::engine>;
    };

}

namespace up
{

    using up_tls::tls;

}
