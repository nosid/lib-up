#pragma once

#include <cstdint>

#include "up_impl_ptr.hpp"
#include "up_stream.hpp"
#include "up_utility.hpp"

namespace up_inet
{

    class invalid_endpoint;


    class ipv4 final
    {
    public: // --- scope ---
        class endpoint;
    };


    // value class for IPv4 endpoints (i.e. IPv4 addresses)
    class ipv4::endpoint final
    {
    public: // --- scope ---
        using self = endpoint;
        class accessor;
        class init;
        class order;
        static const self any;
        static const self loopback;
    private: // --- state ---
        uint8_t _data[4];
    public: // --- life ---
        endpoint() = delete;
        explicit endpoint(init&& argument);
        /// throws invalid_ip_endpoint
        explicit endpoint(const up::string_view& value);
    public: // --- operations ---
        auto to_string() const -> std::string;
    };


    class ipv4::endpoint::order final
    {
    public: // --- scope ---
        static auto prev(const ipv4::endpoint& endpoint) -> ipv4::endpoint;
        static auto next(const ipv4::endpoint& endpoint) -> ipv4::endpoint;
    public: // --- operations ---
        bool operator()(const ipv4::endpoint& lhs, const ipv4::endpoint& rhs) const;
    };


    class ipv6 final
    {
    public: // --- scope ---
        class endpoint;
    };


    // value class for IPv6 endpoints (i.e. IPv6 addresses)
    class ipv6::endpoint final
    {
    public: // --- scope ---
        using self = endpoint;
        class accessor;
        class init;
        class order;
        static const self any;
        static const self loopback;
    private: // --- state ---
        uint8_t _data[16];
    public: // --- life ---
        endpoint() = delete;
        explicit endpoint(init&& argument);
        /// throws invalid_ip_endpoint
        explicit endpoint(const up::string_view& value);
    public: // --- operations ---
        auto to_string() const -> std::string;
    };


    class ipv6::endpoint::order final
    {
    public: // --- scope ---
        static auto prev(const ipv6::endpoint& endpoint) -> ipv6::endpoint;
        static auto next(const ipv6::endpoint& endpoint) -> ipv6::endpoint;
    public: // --- operations ---
        bool operator()(const ipv6::endpoint& lhs, const ipv6::endpoint& rhs) const;
    };


    class ip final
    {
    public: // --- scope ---
        enum class version : uint8_t { v4, v6, };
        class endpoint;
        static auto resolve_canonical(const up::string_view& name) -> std::string;
        static auto resolve_endpoints(const up::string_view& name) -> std::vector<endpoint>;
        static auto resolve_name(const ip::endpoint& endpoint) -> std::string;
    };

    auto to_string(ip::version value) -> std::string;


    class ip::endpoint final
    {
    public: // --- scope ---
        using self = endpoint;
        class accessor;
    private: // --- state ---
        union
        {
            ipv4::endpoint _v4;
            ipv6::endpoint _v6;
        };
        ip::version _version;
    public: // --- life ---
        endpoint() = delete;
        explicit endpoint(const up::string_view& value);
        endpoint(const self& rhs);
        endpoint(const ipv4::endpoint& rhs);
        endpoint(const ipv6::endpoint& rhs);
        endpoint(self&& rhs) noexcept;
        ~endpoint() noexcept;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self&;
        auto operator=(self&& rhs) & noexcept -> self&;
        auto operator=(const ipv4::endpoint& rhs) & -> self&;
        auto operator=(const ipv6::endpoint& rhs) & -> self&;
        auto to_string() const -> std::string;
        auto version() const noexcept -> ip::version;
        explicit operator const ipv4::endpoint&() const;
        explicit operator const ipv6::endpoint&() const;
        explicit operator ipv4::endpoint&();
        explicit operator ipv6::endpoint&();
    };


    class tcp final
    {
    public: // --- scope ---
        enum class port : uint16_t { any = 0, };
        class endpoint;
        class invalid_service;
        class connection;
        class listener;
        class socket;
        // raises invalid_service
        static auto resolve_name(port port) -> std::string;
        // raises invalid_service
        static auto resolve_port(const up::string_view& name) -> port;
    };

    auto to_string(tcp::port value) -> std::string;


    // value class for TCP endpoints (consisting of IP address and port)
    class tcp::endpoint final
    {
    public: // --- scope ---
        using self = endpoint;
        static const self any;
    private: // --- state ---
        ip::endpoint _address;
        tcp::port _port;
    public: // --- life ---
        // implicit constructor
        endpoint(ip::endpoint address, tcp::port port)
            : _address(std::move(address)), _port(std::move(port))
        { }
    public: // --- operations ---
        auto to_fabric() const -> up::fabric;
        auto address() const -> const ip::endpoint& { return _address; }
        auto port() const -> tcp::port { return _port; }
    };


    class tcp::connection final : public up::stream
    {
    public: // --- scope ---
        using self = connection;
        enum class qos_priority : uint8_t { class1, class2, class3, class4, };
        enum class qos_drop : uint8_t { low, med, high, };
        class engine;
    public: // --- life ---
        explicit connection(std::unique_ptr<engine> engine);
    public: // --- operations ---
        auto to_fabric() const -> up::fabric;
        auto local() const -> tcp::endpoint;
        auto remote() const -> const tcp::endpoint&;
        void qos(qos_priority priority, qos_drop drop) const;
        void keepalive(std::chrono::seconds idle, std::size_t probes, std::chrono::seconds interval) const;
        auto incoming_cpu() const -> int;
    private:
        // classes with vtables should have at least one out-of-line virtual method definition
        __attribute__((unused))
        virtual void _vtable_dummy() const override;
    };


    class tcp::listener final
    {
    public: // --- scope ---
        using self = listener;
        class impl;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, destroy> _impl;
    public: // --- life ---
        explicit listener(up::impl_ptr<impl, destroy> impl);
        listener(const self& rhs) = delete;
        listener(self&& rhs) noexcept = default;
        ~listener() noexcept = default;
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
        auto to_fabric() const -> up::fabric;
        auto accept(up::stream::patience& patience) -> connection;
        auto accept(up::stream::patience&& patience) -> connection
        {
            return accept(patience);
        }
    };


    class tcp::socket final
    {
    public: // --- scope ---
        using self = socket;
        enum class option : uint8_t { reuseaddr, reuseport, freebind, };
        using options = up::enum_set<option>;
        class impl;
        static void destroy(impl* ptr);
    private: // --- state ---
        up::impl_ptr<impl, destroy> _impl;
    public: // --- life ---
        explicit socket(ip::version version); // unbound socket
        explicit socket(const tcp::endpoint& endpoint, options options); // bound socket
        socket(const self& rhs) = delete;
        socket(self&& rhs) noexcept = default;
        ~socket() noexcept = default;
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
        auto to_fabric() const -> up::fabric;
        auto endpoint() const -> const tcp::endpoint&;
        auto connect(const tcp::endpoint& remote, up::stream::patience& patience) && -> connection;
        auto connect(const tcp::endpoint& remote, up::stream::patience&& patience) && -> connection
        {
            return std::move(*this).connect(remote, patience);
        }
        auto listen(int backlog) && -> listener;
    };


    class udp final
    {
    public: // --- scope ---
        enum class port : uint16_t { any = 0, };
        class invalid_service;
        // raises invalid_service
        static auto resolve_name(port port) -> std::string;
        // raises invalid_service
        static auto resolve_port(const up::string_view& name) -> port;
    };

    auto to_string(udp::port value) -> std::string;

}

namespace up
{

    using up_inet::ipv4;
    using up_inet::ipv6;
    using up_inet::ip;
    using up_inet::tcp;
    using up_inet::udp;

}
