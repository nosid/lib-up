#include <iostream>

#include "up_buffer.hpp"
#include "up_exception.hpp"
#include "up_inet.hpp"
#include "up_tls.hpp"

namespace
{

    using namespace std::chrono_literals;

    [[noreturn]]
    __attribute__((unused))
    void echo_server(up::tcp::endpoint endpoint)
    {
        using o = up::tcp::socket::option;
        auto listener = up::tcp::socket(std::move(endpoint), {o::reuseaddr}).listen(1);
        for (;;) {
            auto buffer = up::buffer();
            auto now = up::steady_clock::now();
            auto stream = listener.accept(up::stream::steady_patience(now, 3s));
            auto deadline = up::stream::deadline_patience(now + 30s);
            while (auto count = stream.read_some(buffer.reserve(1 << 14), deadline)) {
                buffer.produce(count);
                while (buffer.available()) {
                    buffer.consume(stream.write_some(buffer, deadline));
                }
            }
        }
    }

    [[noreturn]]
    __attribute__((unused))
    void tls_echo_server(up::tcp::endpoint endpoint)
    {
        using o = up::tcp::socket::option;
        std::string pathname = "/usr/share/doc/libssl-doc/demos/bio/server.pem";
        up::tls::server_context tls(up::tls::identity(pathname, pathname), {});
        // up::tls::secure_context tls(
        //     up::tls::authority::system(),
        //     up::tls::identity(pathname, pathname),
        //     {});
        auto listener = up::tcp::socket(std::move(endpoint), {o::reuseaddr}).listen(1);
        for (;;) {
            auto now = up::steady_clock::now();
            auto deadline = up::stream::deadline_patience(now + 30s);
            auto stream = listener.accept(deadline);
            up::tls::server_context::hostname_callback callback = [](std::string hostname) -> up::tls::server_context& {
                std::cerr << "HOSTNAME:" << hostname << '\n';
                UP_RAISE(up::tls::server_context::accept_hostname, "accept"_sl);
            };
            stream.upgrade([&](std::unique_ptr<up::stream::engine> engine) {
                    return tls.upgrade(std::move(engine), deadline, callback);
                    // return tls.upgrade(std::move(engine), deadline,
                    //     [&](bool preverified, std::size_t depth, const up::tls::certificate& certificate) {
                    //         auto cn = certificate.common_name();
                    //         std::cerr << "VERIFY:" << preverified << ':' << depth << '[' << (cn ? *cn : "none") << "]\n";
                    //         return preverified;
                    //     });
                });
            auto buffer = up::buffer();
            while (auto count = stream.read_some(buffer.reserve(1 << 14), deadline)) {
                buffer.produce(count);
                while (buffer.available()) {
                    buffer.consume(stream.write_some(buffer, deadline));
                }
            }
        }
    }

    __attribute__((unused))
    auto connect(up::ip::endpoint address, const up::string_view& port, up::stream::patience& patience)
        -> up::tcp::connection
    {
        return up::tcp::socket(address.version())
            .connect(up::tcp::endpoint(address, up::tcp::resolve_port(port)), patience);
    }

    __attribute__((unused))
    void http_get(up::stream stream, up::stream::patience& patience)
    {
        std::string request("GET / HTTP/1.0\r\n\r\n");
        stream.write_all(up::chunk::from(request), patience);
        auto buffer = up::buffer();
        while (auto count = stream.read_some(buffer.reserve(1 << 14), patience)) {
            buffer.produce(count);
        }
        stream.graceful_close(patience);
    }

}

int main(int argc, char* argv[])
{
    try {

        std::ios::sync_with_stdio(false);

        // echo_server(up::tcp::endpoint(up::ipv4::endpoint::any, up::tcp::resolve_port("http-alt")));

        if (argc == 2 && std::string(argv[1]) == "server") {
            tls_echo_server(up::tcp::endpoint(up::ipv4::endpoint::any, up::tcp::resolve_port("http-alt")));
        }

        auto now = up::steady_clock::now();
        auto deadline = up::stream::deadline_patience(now + 30s);

        for (auto&& address : up::ip::resolve_endpoints("www.heise.de.")) {
            // skip IPv6 addresses
            if (address.version() == up::ip::version::v6) continue;
            http_get(connect(address, "http", deadline), deadline);
            return EXIT_SUCCESS;
        }

        up::tls::client_context tls(
            /*up::nullopt*/ up::tls::authority::system(),
            up::nullopt,
            {});

        std::string hostname = "www.google.com";
        auto endpoints = up::ip::resolve_endpoints(hostname);
        if (endpoints.empty()) {
            std::cerr << "INVALID HOSTNAME: " << hostname << '\n';
            return EXIT_SUCCESS;
        } else {
            auto stream = up::tcp::socket(endpoints[0].version())
                .connect(up::tcp::endpoint(endpoints[0], up::tcp::port(443)), deadline);
            stream.upgrade([&](std::unique_ptr<up::stream::engine> engine) {
                    return tls.upgrade(std::move(engine), deadline, hostname,
                        [&](bool preverified, std::size_t depth, const up::tls::certificate& certificate) {
                            auto cn = certificate.common_name();
                            std::cerr << "VERIFY:" << preverified << ':' << depth << '[' << (cn ? *cn : "none") << "]\n";
                            return preverified
                                && (depth != 0 || certificate.matches_hostname(hostname));
                        });
                });
            http_get(std::move(stream), deadline);
            return EXIT_SUCCESS;
        }

    } catch (...) {
        up::log_current_exception(std::cerr, "ERROR: ");
        return EXIT_FAILURE;
    }
}
