#include "up_inet.hpp"

#include <cstring>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include "up_defer.hpp"
#include "up_exception.hpp"
#include "up_integral_cast.hpp"
#include "up_terminate.hpp"
#include "up_utility.hpp"

/*
 * Interesting, possible extensions to the current implementation:
 *
 * TCP_INFO: Query information about a TCP connection. Can be used to detect
 * packet loss or other problems with a connection.
 *
 * TCP_QUICKACK: Disable delay of TCP ACKs. Might be interesting for
 * unidirectional TCP connections on the receiving side.
 *
 * TCP_USER_TIMEOUT: Abort connection if transmitted data is not
 * acknowledged. Might be interesting for unidirectional TCP connections on
 * the sending side.
 */

namespace
{

    using namespace up::literals;

    class ai_error_info
    {
    private: // --- state ---
        int _value;
    public: // --- con-/destruction ---
        explicit ai_error_info(int value)
            : _value(value)
        { }
    public: // --- operations ---
        auto to_fabric() const
        {
            return up::fabric(typeid(*this), std::string(::gai_strerror(_value)),
                up::invoke_to_fabric_with_fallback(_value));
        }
    };

    template <typename Type>
    auto byte_order_host_to_network(Type) -> Type = delete;

    auto byte_order_host_to_network(uint16_t value) -> uint16_t
    {
        return htons(value);
    }

    auto byte_order_host_to_network(uint32_t value) -> uint32_t
    {
        return htonl(value);
    }

    template <typename Type>
    auto byte_order_network_to_host(Type) -> Type = delete;

    auto byte_order_network_to_host(uint16_t value) -> uint16_t
    {
        return ntohs(value);
    }

    __attribute__((unused))
    auto byte_order_network_to_host(uint32_t value) -> uint32_t
    {
        return ntohl(value);
    }

    struct runtime;

    enum class address_family : int { v4 = AF_INET, v6 = AF_INET6, };

    auto to_string(address_family value)
    {
        if (value == address_family::v4) {
            return up::invoke_to_string("IPv4"_s);
        } else if (value == address_family::v6) {
            return up::invoke_to_string("IPv6"_s);
        } else {
            return up::invoke_to_string(up::to_underlying_type(value));
        }
    }

    enum class address_length : std::size_t { v4 = INET_ADDRSTRLEN, v6 = INET6_ADDRSTRLEN, };

    auto to_string(address_length value)
    {
        return up::invoke_to_string(up::to_underlying_type(value));
    }

    void ip_address_parse(address_family af, const std::string& text, void* result)
    {
        int rv = ::inet_pton(up::to_underlying_type(af), text.c_str(), result);
        if (rv == 1) {
            // okay
        } else if (rv == 0) {
            UP_RAISE(up_inet::invalid_endpoint, "invalid-ip-address"_s, af, text);
        } else {
            UP_RAISE(runtime, "ip-address-parser-error"_s, af, text, up::errno_info(errno));
        }
    }

    auto ip_address_to_string(address_family af, address_length length, const void* address)
    {
        auto buffer = std::make_unique<char[]>(up::to_underlying_type(length));
        const char* rv = ::inet_ntop(up::to_underlying_type(af), address,
            buffer.get(), up::to_underlying_type(length));
        if (rv != nullptr) {
            return std::string(rv);
        } else {
            UP_RAISE(runtime, "ip-address-conversion-error"_s,
                af, length, up::errno_info(errno));
        }
    }


    template <typename... Tags>
    struct traits;

    template <>
    struct traits<up_inet::tcp>
    {
        static constexpr int sock_type = SOCK_STREAM;
        static constexpr int ni_flags = 0;
    };

    template <>
    struct traits<up_inet::udp>
    {
        static constexpr int sock_type = SOCK_DGRAM;
        static constexpr int ni_flags = NI_DGRAM;
    };


    template <typename Protocol>
    auto resolve_service_name(typename Protocol::port port)
    {
        auto length = NI_MAXSERV;
        auto buffer = std::make_unique<char[]>(length);
        sockaddr_in sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = byte_order_host_to_network(up::to_underlying_type(port));
        int rv = ::getnameinfo(reinterpret_cast<sockaddr*>(&sa), sizeof(sa),
            nullptr, 0, buffer.get(), length, traits<Protocol>::ni_flags);
        if (rv == 0) {
            return std::string(buffer.get());
        } else if (rv == EAI_NONAME) {
            UP_RAISE(typename Protocol::invalid_service,
                "unknown-service-name"_s, port, ai_error_info(rv));
        } else {
            UP_RAISE(runtime, "port-resolver-error"_s, port, ai_error_info(rv));
        }
    }

    auto get_address_family(const sockaddr_in& addr)
    {
        return addr.sin_family;
    }

    auto get_address_family(const sockaddr_in6& addr)
    {
        return addr.sin6_family;
    }

    void check_address_family(address_family expected, address_family actual)
    {
        if (expected != actual) {
            UP_RAISE(runtime, "ip-address-family-mismatch"_s, expected, actual);
        }
    }

    void check_address_length(address_length expected, address_length actual)
    {
        if (expected != actual) {
            UP_RAISE(runtime, "ip-address-size-mismatch"_s, expected, actual);
        }
    }

    template <typename ToSockAddr, typename FromSockAddr>
    auto get_sockaddr(const FromSockAddr* from, address_family family, address_length length)
    {
        auto addr = reinterpret_cast<const ToSockAddr*>(from);
        check_address_family(family,
            up::from_underlying_type<address_family>(get_address_family(*addr)));
        check_address_length(length,
            up::from_underlying_type<address_length>(sizeof(ToSockAddr)));
        return addr;
    }

    template <typename SockAddr>
    auto get_sockaddr(const addrinfo& info)
    {
        return get_sockaddr<SockAddr>(info.ai_addr,
            up::from_underlying_type<address_family>(info.ai_family),
            up::from_underlying_type<address_length>(info.ai_addrlen));
    }

    template <typename Callable>
    auto getaddrinfo_aux(const std::string& host_name, int flags, Callable&& callable)
    {
        addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_ADDRCONFIG | flags;
        hints.ai_family = AF_UNSPEC;
        /* If ai_socktype is not set, getaddrinfo returns a record for each
         * (3) sock type. Setting an arbitrary socket type helps. */
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* ai;
        int rv = ::getaddrinfo(host_name.c_str(), nullptr, &hints, &ai);
        if (rv == 0) {
            std::unique_ptr<addrinfo, void (*)(addrinfo*)> scoped(ai, &::freeaddrinfo);
            return std::forward<Callable>(callable)(ai);
        } else if (rv == EAI_NODATA) {
            ai = nullptr;
            return std::forward<Callable>(callable)(ai);
        } else {
            UP_RAISE(runtime, "host-name-resolver-error"_s,
                host_name, flags, ai_error_info(rv));
        }
    }

    template <typename SockAddr>
    auto getnameinfo_aux(SockAddr& sa)
    {
        auto length = NI_MAXHOST;
        auto buffer = std::make_unique<char[]>(length);
        int rv = ::getnameinfo(reinterpret_cast<const sockaddr*>(&sa), sizeof(sa),
            buffer.get(), length, nullptr, 0, NI_NAMEREQD);
        if (rv == 0) {
            return std::string(buffer.get());
        } else {
            UP_RAISE(runtime, "ip-address-resolver-error"_s, ai_error_info(rv));
        }
    }


    int dscp_table[][3] = {
        // from low-drop to high-drop
        {IPTOS_DSCP_AF11, IPTOS_DSCP_AF12, IPTOS_DSCP_AF13, }, // class 1
        {IPTOS_DSCP_AF21, IPTOS_DSCP_AF22, IPTOS_DSCP_AF23, }, // class 2
        {IPTOS_DSCP_AF31, IPTOS_DSCP_AF32, IPTOS_DSCP_AF33, }, // class 3
        {IPTOS_DSCP_AF41, IPTOS_DSCP_AF42, IPTOS_DSCP_AF43, }, // class 4
    };

    template <std::size_t I, std::size_t J>
    auto dscp_lookup(int (&table)[I][J], std::size_t i, std::size_t j)
    {
        if (i < I && j < J) {
            return table[i][j];
        } else {
            UP_RAISE(runtime, "dscp-index-out-of-range"_s, i, j);
        }
    }

}


static_assert(sizeof(up_inet::ipv4::endpoint) == sizeof(in_addr),
    "size mismatch: in_addr / ipv4::endpoint");

class up_inet::ipv4::endpoint::accessor final
{
public:
    static void copy(const endpoint& source, in_addr& target)
    {
        std::memcpy(&target, &source._data, sizeof(target));
    }
};

class up_inet::ipv4::endpoint::init final
{
public: // --- state ---
    const in_addr& _data;
};


auto up_inet::ipv4::endpoint::order::prev(const ipv4::endpoint& endpoint) -> ipv4::endpoint
{
    auto result = endpoint;
    for (auto p = std::begin(result._data), q = std::end(result._data); p != q; ) {
        --q;
        if ((*q)--) {
            break;
        }
    }
    return result;
}

auto up_inet::ipv4::endpoint::order::next(const ipv4::endpoint& endpoint) -> ipv4::endpoint
{
    auto result = endpoint;
    for (auto p = std::begin(result._data), q = std::end(result._data); p != q; ) {
        --q;
        if (++*q) {
            break;
        }
    }
    return result;
}

bool up_inet::ipv4::endpoint::order::operator()(
    const ipv4::endpoint& lhs, const ipv4::endpoint& rhs) const
{
    return std::lexicographical_compare(
        std::begin(lhs._data), std::end(lhs._data),
        std::begin(rhs._data), std::end(rhs._data));
}

const up_inet::ipv4::endpoint up_inet::ipv4::endpoint::any(
    up_inet::ipv4::endpoint::init{in_addr{byte_order_host_to_network(INADDR_ANY)}});
const up_inet::ipv4::endpoint up_inet::ipv4::endpoint::loopback(
    up_inet::ipv4::endpoint::init{in_addr{byte_order_host_to_network(INADDR_LOOPBACK)}});

up_inet::ipv4::endpoint::endpoint(const std::string& value)
{
    ip_address_parse(address_family::v4, value, &_data);
}

up_inet::ipv4::endpoint::endpoint(init&& argument)
{
    std::memcpy(&_data, &argument._data, sizeof(_data));
}

auto up_inet::ipv4::endpoint::to_string() const -> std::string
{
    return ip_address_to_string(address_family::v4, address_length::v4, &_data);
}


static_assert(sizeof(up_inet::ipv6::endpoint) == sizeof(in6_addr),
    "size mismatch: in6_addr / ipv6::endpoint");

class up_inet::ipv6::endpoint::accessor final
{
public:
    static void copy(const endpoint& source, in6_addr& target)
    {
        std::memcpy(&target, &source._data, sizeof(target));
    }
};

class up_inet::ipv6::endpoint::init final
{
public: // --- state ---
    const in6_addr& _data;
};

auto up_inet::ipv6::endpoint::order::prev(const ipv6::endpoint& endpoint) -> ipv6::endpoint
{
    auto result = endpoint;
    for (auto p = std::begin(result._data), q = std::end(result._data); p != q; ) {
        --q;
        if ((*q)--) {
            break;
        }
    }
    return result;
}

auto up_inet::ipv6::endpoint::order::next(const ipv6::endpoint& endpoint) -> ipv6::endpoint
{
    auto result = endpoint;
    for (auto p = std::begin(result._data), q = std::end(result._data); p != q; ) {
        --q;
        if (++*q) {
            break;
        }
    }
    return result;
}

bool up_inet::ipv6::endpoint::order::operator()(
    const ipv6::endpoint& lhs, const ipv6::endpoint& rhs) const
{
    return std::lexicographical_compare(
        std::begin(lhs._data), std::end(lhs._data),
        std::begin(rhs._data), std::end(rhs._data));
}

const up_inet::ipv6::endpoint up_inet::ipv6::endpoint::any(
    up_inet::ipv6::endpoint::init{in6_addr IN6ADDR_ANY_INIT});
const up_inet::ipv6::endpoint up_inet::ipv6::endpoint::loopback(
    up_inet::ipv6::endpoint::init{in6_addr IN6ADDR_LOOPBACK_INIT});

up_inet::ipv6::endpoint::endpoint(const std::string& value)
{
    ip_address_parse(address_family::v6, value, &_data);
}

up_inet::ipv6::endpoint::endpoint(init&& argument)
{
    std::memcpy(&_data, &argument._data, sizeof(_data));
}

auto up_inet::ipv6::endpoint::to_string() const -> std::string
{
    return ip_address_to_string(address_family::v6, address_length::v6, &_data);
}


auto up_inet::ip::resolve_canonical(const std::string& name) -> std::string
{
    return getaddrinfo_aux(name, AI_CANONNAME, [&name](addrinfo* ai) -> std::string {
            if (ai && ai->ai_canonname) {
                return std::string(ai->ai_canonname);
            } else {
                UP_RAISE(runtime, "canonical-host-name-resolver-error"_s, name);
            }
        });
}

auto up_inet::ip::resolve_endpoints(const std::string& name) -> std::vector<endpoint>
{
    return getaddrinfo_aux(name, 0, [](addrinfo* ai) -> std::vector<endpoint> {
            std::vector<endpoint> result;
            for (addrinfo* i = ai; i; i = i->ai_next) {
                if (i->ai_family == AF_INET) {
                    auto addr = get_sockaddr<sockaddr_in>(*i);
                    result.emplace_back(ipv4::endpoint(ipv4::endpoint::init{addr->sin_addr}));
                } else if (i->ai_family == AF_INET6) {
                    auto addr = get_sockaddr<sockaddr_in6>(*i);
                    result.emplace_back(ipv6::endpoint(ipv6::endpoint::init{addr->sin6_addr}));
                } // else: ignore other address families
            }
            return result;
        });
}


auto up_inet::ip::resolve_name(const endpoint& endpoint) -> std::string
{
    if (endpoint.version() == ip::version::v4) {
        sockaddr_in sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        ipv4::endpoint::accessor::copy(static_cast<ipv4::endpoint>(endpoint), sa.sin_addr);
        return getnameinfo_aux(sa);
    } else if (endpoint.version() == ip::version::v6) {
        sockaddr_in6 sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sin6_family = AF_INET6;
        ipv6::endpoint::accessor::copy(static_cast<ipv6::endpoint>(endpoint), sa.sin6_addr);
        return getnameinfo_aux(sa);
    } else {
        UP_RAISE(runtime, "unexpected-tcp-endpoint-ip-address-version"_s, endpoint.version());
    }
}


auto up_inet::to_string(ip::version value) -> std::string
{
    switch (value) {
    case ip::version::v4:
        return up::invoke_to_string("ipv4"_s);
    case ip::version::v6:
        return up::invoke_to_string("ipv6"_s);
    }
    UP_RAISE(runtime, "unexpected-ip-version"_s, up::to_underlying_type(value));
}


up_inet::ip::endpoint::endpoint(const std::string& value)
    : _version(ip::version::v4)
{
    try {
        new (&_v4) ipv4::endpoint(value);
    } catch (const up::exception<invalid_endpoint>&) {
        _version = ip::version::v6;
        new (&_v6) ipv6::endpoint(value);
    }
}

up_inet::ip::endpoint::endpoint(const endpoint& rhs)
    : _version(rhs._version)
{
    switch (_version) {
    case ip::version::v4:
        new (&_v4) ipv4::endpoint(rhs._v4);
        return;
    case ip::version::v6:
        new (&_v6) ipv6::endpoint(rhs._v6);
        return;
    }
    UP_RAISE(runtime, "unexpected-tcp-endpoint-ip-address-version"_s, _version);
}

up_inet::ip::endpoint::endpoint(const ipv4::endpoint& rhs)
    : _v4(rhs), _version(ip::version::v4)
{ }

up_inet::ip::endpoint::endpoint(const ipv6::endpoint& rhs)
    : _v6(rhs), _version(ip::version::v6)
{ }

up_inet::ip::endpoint::endpoint(endpoint&& rhs) noexcept
    : _version(rhs._version)
{
    switch (_version) {
    case ip::version::v4:
        new (&_v4) ipv4::endpoint(std::move(rhs._v4));
        return;
    case ip::version::v6:
        new (&_v6) ipv6::endpoint(std::move(rhs._v6));
        return;
    }
    UP_RAISE(runtime, "unexpected-tcp-endpoint-ip-address-version"_s, _version);
}

up_inet::ip::endpoint::~endpoint() noexcept
{
    switch (_version) {
    case ip::version::v4:
        _v4.~endpoint();
        return;
    case ip::version::v6:
        _v6.~endpoint();
        return;
    }
    UP_RAISE(runtime, "unexpected-tcp-endpoint-ip-address-version"_s, _version);
}

auto up_inet::ip::endpoint::operator=(const self& rhs) & -> self&
{
    switch (rhs._version) {
    case ip::version::v4:
        return operator=(rhs._v4);
    case ip::version::v6:
        return operator=(rhs._v6);
    }
    UP_RAISE(runtime, "unexpected-tcp-endpoint-ip-address-version"_s, rhs._version);
}

auto up_inet::ip::endpoint::operator=(self&& rhs) & noexcept -> self&
{
    // uses copy instead of move - because there is no difference
    switch (rhs._version) {
    case ip::version::v4:
        return operator=(rhs._v4);
    case ip::version::v6:
        return operator=(rhs._v6);
    }
    UP_RAISE(runtime, "unexpected-tcp-endpoint-ip-address-version"_s, rhs._version);
}

auto up_inet::ip::endpoint::operator=(const ipv4::endpoint& rhs) & -> self&
{
    switch (_version) {
    case ip::version::v4:
        _v4 = rhs;
        return *this;
    case ip::version::v6:
        _v6.~endpoint();
        static_assert(noexcept(ipv4::endpoint(rhs)), "requires noexcept");
        new (&_v4) ipv4::endpoint(rhs);
        return *this;
    }
    UP_RAISE(runtime, "unexpected-tcp-endpoint-ip-address-version"_s, _version);
}

auto up_inet::ip::endpoint::operator=(const ipv6::endpoint& rhs) & -> endpoint&
{
    switch (_version) {
    case ip::version::v4:
        _v4.~endpoint();
        static_assert(noexcept(ipv6::endpoint(rhs)), "requires noexcept");
        new (&_v6) ipv6::endpoint(rhs);
        return *this;
    case ip::version::v6:
        _v6 = rhs;
        return *this;
    }
    UP_RAISE(runtime, "unexpected-tcp-endpoint-ip-address-version"_s, _version);
}

auto up_inet::ip::endpoint::to_string() const -> std::string
{
    switch (_version) {
    case ip::version::v4:
        return _v4.to_string();
    case ip::version::v6:
        return _v6.to_string();
    }
    UP_RAISE(runtime, "unexpected-tcp-endpoint-ip-address-version"_s, _version);
}

auto up_inet::ip::endpoint::version() const noexcept -> ip::version
{
    return _version;
}

up_inet::ip::endpoint::operator const ipv4::endpoint&() const
{
    if (_version == ip::version::v4) {
        return _v4;
    } else {
        throw std::bad_cast();
    }
}

up_inet::ip::endpoint::operator const ipv6::endpoint&() const
{
    if (_version == ip::version::v6) {
        return _v6;
    } else {
        throw std::bad_cast();
    }
}

up_inet::ip::endpoint::operator ipv4::endpoint&()
{
    if (_version == ip::version::v4) {
        return _v4;
    } else {
        throw std::bad_cast();
    }
}

up_inet::ip::endpoint::operator ipv6::endpoint&()
{
    if (_version == ip::version::v6) {
        return _v6;
    } else {
        throw std::bad_cast();
    }
}


namespace
{

    template <typename Protocol>
    auto resolve_service_port(const std::string& name)
    {
        addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_ADDRCONFIG;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = traits<Protocol>::sock_type;
        addrinfo* ai;
        int rv = ::getaddrinfo(nullptr, name.c_str(), &hints, &ai);
        if (rv == EAI_NONAME) {
            UP_RAISE(typename Protocol::invalid_service, "unknown-network-service"_s,
                name, ai_error_info(rv));
        } else if (rv != 0) {
            UP_RAISE(runtime, "port-resolver-error"_s, name, ai_error_info(rv));
        } // else: okay and continue
        UP_DEFER { ::freeaddrinfo(ai); };
        using port_t = typename Protocol::port;
        port_t port = {};
        std::size_t count = 0;
        auto&& process = [&](uint16_t in_port) {
            auto current = up::from_underlying_type<port_t>(
                byte_order_network_to_host(in_port));
            if (count == 0 || port == current) {
                port = current;
                ++count;
            } else {
                UP_RAISE(runtime, "protocol-service-port-mismatch"_s, name, port, current);
            }
        };
        for (addrinfo* i = ai; i; i = i->ai_next) {
            if (i->ai_socktype == traits<Protocol>::sock_type) {
                if (i->ai_family == AF_INET) {
                    process(get_sockaddr<sockaddr_in>(*i)->sin_port);
                } else if (i->ai_family == AF_INET6) {
                    process(get_sockaddr<sockaddr_in6>(*i)->sin6_port);
                } // else: ignore other address families
            }
        }
        return port;
    }

    void close_aux(int& fd)
    {
        if (fd != -1) {
            int temp = std::exchange(fd, -1);
            int rv = ::close(temp);
            if (rv != 0) {
                UP_TERMINATE("bad-close"_s, temp);
            }
        } // else: nothing
    }


    auto make_tcp_endpoint(const sockaddr_storage* addr, socklen_t length)
    {
        auto l = up::from_underlying_type<address_length>(length);
        if (length > sizeof(*addr)) {
            UP_RAISE(runtime, "invalid-endpoint-address-size"_s,
                l, up::from_underlying_type<address_length>(sizeof(*addr)));
        } else if (addr->ss_family == AF_INET) {
            auto* a = get_sockaddr<sockaddr_in>(addr, address_family::v4, l);
            auto p = up::from_underlying_type<up_inet::tcp::port>(a->sin_port);
            return up_inet::tcp::endpoint(
                up_inet::ipv4::endpoint(up_inet::ipv4::endpoint::init{a->sin_addr}), p);
        } else if (addr->ss_family == AF_INET6) {
            auto* a = get_sockaddr<sockaddr_in6>(addr, address_family::v6, l);
            auto p = up::from_underlying_type<up_inet::tcp::port>(a->sin6_port);
            return up_inet::tcp::endpoint(
                up_inet::ipv6::endpoint(up_inet::ipv6::endpoint::init{a->sin6_addr}), p);
        } else {
            UP_RAISE(runtime, "unexpected-ip-address-family"_s,
                up::from_underlying_type<address_family>(addr->ss_family));
        }
    }


    template <typename Callable>
    auto identify_tcp_endpoint(Callable&& callable, int fd)
    {
        sockaddr_storage addr;
        socklen_t length = sizeof(addr);
        int rv = callable(fd, reinterpret_cast<sockaddr*>(&addr), &length);
        if (rv == 0) {
            return make_tcp_endpoint(&addr, length);
        } else {
            UP_RAISE(runtime, "endpoint-identification-error"_s, up::errno_info(errno));
        }
    }


    template <typename Callable>
    auto with_sockaddr(const up_inet::tcp::endpoint& endpoint, Callable&& callable)
    {
        if (endpoint.address().version() == up_inet::ip::version::v4) {
            sockaddr_in sa;
            std::memset(&sa, 0, sizeof(sa));
            sa.sin_family = AF_INET;
            sa.sin_port = byte_order_host_to_network(up::to_underlying_type(endpoint.port()));
            up_inet::ipv4::endpoint::accessor::copy(
                static_cast<up_inet::ipv4::endpoint>(endpoint.address()), sa.sin_addr);
            return std::forward<Callable>(callable)(
                reinterpret_cast<const sockaddr*>(&sa), sizeof(sa));
        } else if (endpoint.address().version() == up_inet::ip::version::v6) {
            sockaddr_in6 sa;
            std::memset(&sa, 0, sizeof(sa));
            sa.sin6_family = AF_INET6;
            sa.sin6_port = byte_order_host_to_network(up::to_underlying_type(endpoint.port()));
            up_inet::ipv6::endpoint::accessor::copy(
                static_cast<up_inet::ipv6::endpoint>(endpoint.address()), sa.sin6_addr);
            return std::forward<Callable>(callable)(
                reinterpret_cast<const sockaddr*>(&sa), sizeof(sa));
        } else {
            UP_RAISE(runtime, "unexpected-tcp-endpoint-ip-address-version"_s,
                endpoint.address().version());
        }
    }

    template <typename Unavailable, typename Operation, typename... Args>
    auto do_transfer(Operation&& operation, Args&&... args) -> std::size_t
    {
        bool restarted = false;
        for (;;) {
            ssize_t rv = operation();
            if (rv != -1) {
                return rv;
            } else if (errno == EINTR && !restarted) {
                /* restart: This case should not happen at all, because
                 * non-blocking sockets are used. However, one retry is
                 * supported in case it happens nevertheless. */
                restarted = true;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                UP_RAISE(Unavailable, std::forward<Args>(args)..., up::errno_info(errno));
            } else {
                UP_RAISE(runtime, std::forward<Args>(args)..., up::errno_info(errno));
            }
        }
    }

}


auto up_inet::tcp::resolve_name(port port) -> std::string
{
    return resolve_service_name<tcp>(port);
}

auto up_inet::tcp::resolve_port(const std::string& name) -> port
{
    return resolve_service_port<tcp>(name);
}

auto up_inet::to_string(tcp::port value) -> std::string
{
    return up::invoke_to_string(up::to_underlying_type(value));
}


const up_inet::tcp::endpoint up_inet::tcp::endpoint::any(ipv4::endpoint::any, tcp::port::any);


auto up_inet::tcp::endpoint::to_fabric() const -> up::fabric
{
    return up::fabric(typeid(*this), "tcp-endpoint",
        up::invoke_to_fabric_with_fallback(_address),
        up::invoke_to_fabric_with_fallback(_port));
}


class up_inet::tcp::socket::impl final
{
public: // --- scope ---
    using self = impl;
public: // --- state ---
    tcp::endpoint _endpoint;
    /* Socket descriptor. It should always be non-blocking, because there is
     * no benefit in real applications for blocking sockets. */
    int _fd;
public: // --- life ---
    explicit impl(const tcp::endpoint& endpoint, ip::version version)
        : _endpoint(endpoint), _fd(-1)
    {
        int domain = 0;
        if (version == ip::version::v4) {
            domain = AF_INET;
        } else if (version == ip::version::v6) {
            domain = AF_INET6;
        } else {
            UP_RAISE(runtime, "unexpected-tcp-endpoint-ip-address-version"_s, version);
        }
        _fd = ::socket(domain, traits<tcp>::sock_type | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (_fd == -1) {
            UP_RAISE(runtime, "tcp-socket-creation-error"_s, version, up::errno_info(errno));
        }
    }
    explicit impl(const tcp::endpoint& endpoint, int fd)
        : _endpoint(endpoint), _fd(fd)
    { }
    impl(const self& rhs) = delete;
    impl(self&& rhs) noexcept = delete;
    ~impl() noexcept
    {
        close_aux(_fd);
    }
public: // --- operations ---
    auto operator=(const self& rhs) & -> self& = delete;
    auto operator=(self&& rhs) & noexcept -> self& = delete;
    template <typename Type>
    void setsockopt(int level, int option, Type&& value)
    {
        int rv = ::setsockopt(_fd, level, option, &value, sizeof(value));
        if (rv != 0) {
            UP_RAISE(runtime, "network-socket-option-error"_s,
                _fd, level, option, up::errno_info(errno));
        }
    }
    auto to_fabric() const -> up::fabric
    {
        return up::fabric(typeid(*this), "tcp-socket-impl",
            up::invoke_to_fabric_with_fallback(_endpoint),
            up::invoke_to_fabric_with_fallback(_fd));
    }
    void hard_close(bool reset = false)
    {
        if (_fd == -1) {
            UP_RAISE(runtime, "invalid-socket-state"_s);
        } else if (reset) {
            /* The socket is closed immediately. Most likely this causes a
             * connection reset on the other end. */
            setsockopt(SOL_SOCKET, SO_LINGER, linger{0, 0});
            close_aux(_fd);
        } else {
            close_aux(_fd);
        }
    }
    auto get_native_handle() const
    {
        return up::stream::native_handle(_fd);
    }
};


class up_inet::tcp::connection::engine final : public up::stream::engine
{
public: // --- scope ---
    using self = engine;
public: // --- state ---
    std::shared_ptr<socket::impl> _socket;
    tcp::endpoint _remote;
public: // --- life ---
    explicit engine(std::shared_ptr<socket::impl>&& socket, tcp::endpoint remote)
        : _socket(std::move(socket)), _remote(std::move(remote))
    { }
    engine(const self& rhs) = delete;
    engine(self&& rhs) noexcept = delete;
    ~engine() noexcept override
    {
        if (_socket->_fd != -1) {
            _socket->hard_close(true);
        }
    }
public: // --- operations ---
    auto operator=(const self& rhs) & -> self& = delete;
    auto operator=(self&& rhs) & noexcept -> self& = delete;
    auto to_fabric() const -> up::fabric
    {
        return up::fabric(typeid(*this), "tcp-connection-engine",
            up::invoke_to_fabric_with_fallback(*_socket),
            up::invoke_to_fabric_with_fallback(_remote));
    }
private:
    void shutdown() const override
    {
        /* We only provide a way to close the sending part of the socket. It
         * is unclear, if SHUT_RD has any use at all. At least for TCP, it
         * seems to have no effect at all. */
        int rv = ::shutdown(_socket->_fd, SHUT_WR);
        if (rv != 0) {
            UP_RAISE(runtime, "tcp-connection-shutdown-error"_s, _remote, up::errno_info(errno));
        } // else: ok
    }
    void hard_close() const override
    {
        _socket->hard_close();
    }
    auto read_some(up::chunk::into chunk) const -> std::size_t override
    {
        return do_transfer<up::stream::engine::unreadable>(
            [&]() { return ::recv(_socket->_fd, chunk.data(), chunk.size(), 0); },
            "tcp-connection-read-error"_s, _remote, chunk.size());
    }
    auto write_some(up::chunk::from chunk) const -> std::size_t override
    {
        return do_transfer<up::stream::engine::unwritable>(
            [&]() { return ::send(_socket->_fd, chunk.data(), chunk.size(), MSG_NOSIGNAL); },
            "tcp-connection-write-error"_s, _remote, chunk.size());
    }
    auto read_some_bulk(up::chunk::into_bulk_t& chunks) const -> std::size_t override
    {
        return do_transfer<up::stream::engine::unreadable>(
            [&]() {
                msghdr msg = {
                    .msg_name = nullptr,
                    .msg_namelen = 0,
                    .msg_iov = chunks.as<iovec>(),
                    .msg_iovlen = up::integral_caster(chunks.count()),
                    .msg_control = nullptr,
                    .msg_controllen = 0,
                    .msg_flags = 0,
                };
                return ::recvmsg(_socket->_fd, &msg, 0);
            },
            "tcp-connection-readv-error"_s, _remote, chunks.count(), chunks.total());
    }
    auto write_some_bulk(up::chunk::from_bulk_t& chunks) const -> std::size_t override
    {
        return do_transfer<up::stream::engine::unwritable>(
            [&]() {
                msghdr msg = {
                    .msg_name = nullptr,
                    .msg_namelen = 0,
                    .msg_iov = chunks.as<iovec>(),
                    .msg_iovlen = up::integral_caster(chunks.count()),
                    .msg_control = nullptr,
                    .msg_controllen = 0,
                    .msg_flags = 0,
                };
                return ::sendmsg(_socket->_fd, &msg, MSG_NOSIGNAL);
            },
            "tcp-connection-writev-error"_s, _remote, chunks.count(), chunks.total());
    }
    auto downgrade() -> up::impl_ptr<up::stream::engine> override
    {
        UP_RAISE(runtime, "tcp-bad-downgrade-error"_s);
    }
    auto get_underlying_engine() const -> const engine* override
    {
        return this;
    }
    auto get_native_handle() const -> up::stream::native_handle override
    {
        return _socket->get_native_handle();
    }
};


up_inet::tcp::connection::connection(up::impl_ptr<engine> engine)
    : stream(std::move(engine))
{ }

auto up_inet::tcp::connection::to_fabric() const -> up::fabric
{
    return static_cast<const engine*>(get_underlying_engine())->to_fabric();
}

auto up_inet::tcp::connection::local() const -> tcp::endpoint
{
    return identify_tcp_endpoint(
        ::getsockname,
        static_cast<const engine*>(get_underlying_engine())->_socket->_fd);
}

auto up_inet::tcp::connection::remote() const -> const tcp::endpoint&
{
    return static_cast<const engine*>(get_underlying_engine())->_remote;
}

void up_inet::tcp::connection::qos(qos_priority priority, qos_drop drop) const
{
    int value = dscp_lookup(dscp_table, up::to_underlying_type(priority), up::to_underlying_type(drop));
    static_cast<const engine*>(get_underlying_engine())->_socket->setsockopt(IPPROTO_IP, IP_TOS, value);
}

void up_inet::tcp::connection::keepalive(std::chrono::seconds idle, std::size_t probes, std::chrono::seconds interval) const
{
    auto&& socket = *static_cast<const engine*>(get_underlying_engine())->_socket;
    socket.setsockopt(SOL_SOCKET, SO_KEEPALIVE, int(1));
    socket.setsockopt(IPPROTO_TCP, TCP_KEEPIDLE, up::integral_cast<int>(idle.count()));
    socket.setsockopt(IPPROTO_TCP, TCP_KEEPCNT, up::integral_cast<int>(probes));
    socket.setsockopt(IPPROTO_TCP, TCP_KEEPINTVL, up::integral_cast<int>(interval.count()));
}


class up_inet::tcp::listener::impl final
{
public: // --- scope ---
    using self = impl;
public: // --- state ---
    up::impl_ptr<socket::impl> _socket;
public: // --- life ---
    explicit impl(up::impl_ptr<socket::impl>&& socket, int backlog)
        : _socket(std::move(socket))
    {
        int rv = ::listen(_socket->_fd, backlog);
        if (rv != 0) {
            UP_RAISE(runtime, "tcp-socket-listen-error"_s, backlog, up::errno_info(errno));
        }
    }
    impl(const self& rhs) = delete;
    impl(self&& rhs) noexcept = delete;
    ~impl() noexcept = default;
public: // --- operations ---
    auto operator=(const self& rhs) & -> self& = delete;
    auto operator=(self&& rhs) & noexcept -> self& = delete;
    auto to_fabric() const -> up::fabric
    {
        return up::fabric(typeid(*this), "tcp-listener-impl",
            up::invoke_to_fabric_with_fallback(*_socket));
    }
};


up_inet::tcp::listener::listener(up::impl_ptr<impl> impl)
    : _impl(std::move(impl))
{ }

auto up_inet::tcp::listener::to_fabric() const -> up::fabric
{
    return _impl->to_fabric();
}

auto up_inet::tcp::listener::accept(up::stream::await& awaiting) -> connection
{
    bool waited = false;
    sockaddr_storage addr;
    socklen_t length = sizeof(addr);
    for (;;) {
        /* Note: accept can be executed by several threads. However, the
         * implementation is not fair. A better approach is to open several
         * sockets with SO_REUSEPORT (since linux 3.9). This is also possible
         * with different processes. */
        auto&& socket = std::make_shared<socket::impl>(_impl->_socket->_endpoint, -1);
        int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
        socket->_fd = ::accept4(_impl->_socket->_fd, reinterpret_cast<sockaddr*>(&addr), &length, flags);
        if (socket->_fd != -1) {
            socket->setsockopt(IPPROTO_TCP, TCP_NODELAY, int(1));
            return connection(up::make_impl<connection::engine>(std::move(socket), make_tcp_endpoint(&addr, length)));
        } else if (!waited && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            awaiting(_impl->_socket->get_native_handle(), up::stream::await::operation::read);
            waited = true;
        } else if (errno == EINTR) {
            // restart
        } else {
            UP_RAISE(runtime, "tcp-listener-accept-error"_s, _impl->_socket->_endpoint, up::errno_info(errno));
        }
    }
}


up_inet::tcp::socket::socket(ip::version version)
    : _impl(up::make_impl<impl>(endpoint::any, version))
{ }

up_inet::tcp::socket::socket(const tcp::endpoint& endpoint, up::enum_set<option> options)
    : _impl(up::make_impl<impl>(endpoint, endpoint.address().version()))
{
    if (options.all(option::reuseaddr)) {
        _impl->setsockopt(SOL_SOCKET, SO_REUSEADDR, int(1));
    }
    if (options.all(option::reuseport)) {
        _impl->setsockopt(SOL_SOCKET, SO_REUSEPORT, int(1));
    }
    if (options.all(option::freebind)) {
        _impl->setsockopt(IPPROTO_IP, IP_FREEBIND, int(1));
    }
    if (endpoint.address().version() == ip::version::v6) {
        _impl->setsockopt(IPPROTO_IPV6, IPV6_V6ONLY, int(1));
    }
    int rv = with_sockaddr(endpoint, [fd=_impl->_fd](const sockaddr* addr, socklen_t addrlen) {
            return ::bind(fd, addr, addrlen);
        });
    if (rv != 0) {
        UP_RAISE(runtime, "tcp-socket-bind-error"_s, endpoint, options, up::errno_info(errno));
    }
}

auto up_inet::tcp::socket::to_fabric() const -> up::fabric
{
    return _impl->to_fabric();
}

auto up_inet::tcp::socket::endpoint() const -> const tcp::endpoint&
{
    return _impl->_endpoint;
}

auto up_inet::tcp::socket::connect(const tcp::endpoint& remote, up::stream::await& awaiting) &&
    -> connection
{
    with_sockaddr(remote, [&](const sockaddr* addr, socklen_t addrlen) {
        for (;;) {
            int rv = ::connect(_impl->_fd, addr, addrlen);
            if (rv == 0) {
                // okay
                return;
            } else if (errno == EINTR) {
                // restart
            } else if (errno == EINPROGRESS) {
                awaiting(_impl->get_native_handle(), up::stream::await::operation::write);
                int error = 0;
                socklen_t length = sizeof(error);
                int rv = ::getsockopt(_impl->_fd, SOL_SOCKET, SO_ERROR, &error, &length);
                if (rv != 0) {
                    UP_RAISE(runtime, "tcp-socket-connect-error"_s, remote, up::errno_info(errno));
                } else if (length != sizeof(error)) {
                    UP_RAISE(runtime, "tcp-socket-connect-status-length-mismatch"_s,
                        remote, length, sizeof(error));
                } else if (error != 0) {
                    UP_RAISE(runtime, "tcp-socket-connect-error"_s, remote, up::errno_info(error));
                } else {
                    return;
                }
            } else {
                UP_RAISE(runtime, "tcp-socket-connect-failed"_s, remote, up::errno_info(errno));
            }
        }
    });
    _impl->setsockopt(IPPROTO_TCP, TCP_NODELAY, int(1));
    return connection(up::make_impl<connection::engine>(std::move(_impl), remote));
}

auto up_inet::tcp::socket::listen(int backlog) && -> listener
{
    return listener(up::make_impl<listener::impl>(std::move(_impl), backlog));
}


auto up_inet::udp::resolve_name(port port) -> std::string
{
    return resolve_service_name<udp>(port);
}

auto up_inet::udp::resolve_port(const std::string& name) -> port
{
    return resolve_service_port<udp>(name);
}

auto up_inet::to_string(udp::port value) -> std::string
{
    return up::invoke_to_string(up::to_underlying_type(value));
}
