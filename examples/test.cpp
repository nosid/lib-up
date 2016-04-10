#include <iostream>
#include <sstream>

#include "up_defer.hpp"
#include "up_exception.hpp"
#include "up_fs.hpp"
#include "up_xml.hpp"

namespace
{

    auto load(const up::fs::file& file)
    {
        auto buffer = up::buffer();
        auto offset = std::size_t(0);
        while (auto count = file.read_some(buffer.reserve(1 << 14), offset)) {
            offset += count;
            buffer.produce(count);
        }
        return buffer;
    }

    __attribute__((unused))
    auto dump(std::ostream& os, const up::buffer& buffer) -> std::ostream&
    {
        return os.write(buffer.warm(), buffer.available());
    }

    void dump(std::ostream& os, const up::xml::element& element)
    {
        up::out(os, '<', element.tag().local_name(), '>', element.head());
        for (auto&& child : element.elements()) {
            dump(os, child);
        }
        up::out(os, "</", element.tag().local_name(), '>', element.tail());
    }

}

int main(int argc, char* argv[])
{
    try {

        std::ios::sync_with_stdio(false);

        auto origin = up::fs::origin(up::fs::context("root"));
        auto&& p = [origin,follow=false](up::shared_string pathname) {
            return up::fs::location(origin, std::move(pathname), follow);
        };
        for (int i = 1; i < argc; ++i) {
            std::cout << "FILE: " << argv[i] << '\n';
            using o = up::fs::file::option;
            auto document = up::xml::document(
                load(up::fs::file(p(argv[i]), {o::read})),
                {}, {}, up::xml::null_uri_loader(), {});
            dump(std::cout, document.to_element());
            std::cout << '\n';
            // auto stylesheet = up::xml::stylesheet(document);
            // document = stylesheet(document);
            // dump(std::cout, document.serialize()) << "\n\n";
        }

    } catch (...) {
        up::log_current_exception(std::cerr, "ERROR: ");
        return EXIT_FAILURE;
    }
}
