#include "up_include.hpp"

#include <iostream>

#include "up_buffer.hpp"
#include "up_exception.hpp"
#include "up_inet.hpp"
#include "up_tls.hpp"

namespace
{

    using namespace std::chrono_literals;

    __attribute__((unused))
    void echo_server(up::tcp::endpoint endpoint)
    {
        using o = up::tcp::socket::option;
        auto listener = up::tcp::socket(std::move(endpoint), {o::reuseaddr}).listen(1);
        for (;;) {
            auto buffer = up::buffer();
            auto now = up::steady_clock::now();
            auto connection = listener.accept(up::stream::steady_await(now, 3s));
            auto deadline = up::stream::deadline_await(now + 30s);
            while (auto count = connection.read_some(buffer.reserve(1 << 14), deadline)) {
                buffer.produce(count);
                while (buffer.available()) {
                    buffer.consume(connection.write_some(buffer, deadline));
                }
            }
        }
    }

    __attribute__((unused))
    auto connect(up::ip::endpoint address, const std::string& port, up::stream::await& await)
        -> up::tcp::connection
    {
        return up::tcp::socket(address.version())
            .connect(up::tcp::endpoint(address, up::tcp::resolve_port(port)), await);
    }

    __attribute__((unused))
    void http_get(up::tcp::connection connection, up::stream::await& await)
    {
        std::string request("GET / HTTP/1.0\r\n\r\n");
        connection.write_some(request, await);
        auto buffer = up::buffer();
        while (auto count = connection.read_some(buffer.reserve(1 << 14), await)) {
            buffer.produce(count);
        }
        connection.graceful_close(await);
    }

}

int main()
{
    try {

        // echo_server(up::tcp::endpoint(up::ipv4::endpoint::any, up::tcp::resolve_port("http-alt")));

        auto now = up::steady_clock::now();
        auto deadline = up::stream::deadline_await(now + 3000ms);

        for (auto&& address : up::ip::resolve_endpoints("www.heise.de.")) {
            // skip IPv6 addresses
            if (address.version() == up::ip::version::v6) continue;
            http_get(connect(address, "http", deadline), deadline);
        }

        // up::tls::context tls(
        //     up::tls::authority::system(),
        //     up::tls::identity("XXX", "XXX"));

        // for (auto&& address : up::ip::resolve_endpoints("www.heise.de.")) {
        //     std::cerr << "ip address: " << address.to_string() << '\n';
        //     // skip IPv6 addresses
        //     if (address.version() == up::ip::version::v6) continue;
        //     auto connection = connect(address, "http", deadline);
        //     // connection.upgrade([&](up::impl_ptr<up::stream::engine> engine) {
        //     //         return tls.make_engine(std::move(engine), deadline);
        //     //     });
        //     http_get(std::move(connection), deadline);
        // }

    } catch (...) {
        up::log_current_exception(std::cerr, "ERROR: ");
        return EXIT_FAILURE;
    }
}
