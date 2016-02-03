#include <cstring>

#include "up_char_cast.hpp"
#include "up_exception.hpp"
#include "up_test.hpp"
#include "up_zlib.hpp"

namespace
{

    struct runtime;

    /* TODO: Add a new module that supports commonly used codecs. */
    class bytes final
    {
    public: // --- scope ---
        using self = bytes;
        static auto raw(up::buffer buffer) -> self
        {
            return self(std::move(buffer));
        }
        static auto hex(const char* text) -> self
        {
            std::size_t size = std::strlen(text);
            if (size % 2 != 0) {
                up::raise<runtime>("bad-hex-size", size);
            }
            up::buffer buffer;
            buffer.reserve(size / 2);
            auto cold = buffer.cold();
            for (std::size_t i = 0; i != size; i += 2) {
                cold[i / 2] = (_hex_value(text[i]) << 4) + _hex_value(text[i + 1]);
            }
            buffer.produce(size / 2);
            return self(std::move(buffer));
        }
        static auto _hex_value(char c) -> char
        {
            if (c >= '0' && c <= '9') {
                return c - '0';
            } else if (c >= 'A' && c <= 'F') {
                return c - 'A' + 10;
            } else if (c >= 'a' && c <= 'f') {
                return c - 'a' + 10;
            } else {
                up::raise<runtime>("bad-hex-char", c);
            }
        }
    private: // --- fields ---
        up::buffer _buffer;
    private: // --- life ---
        explicit bytes(up::buffer buffer)
            : _buffer(std::move(buffer))
        { }
    public: // --- operations ---
        auto to_string() const -> std::string
        {
            const char* chars = "0123456789abcdef";
            unsigned char _16 = 16;
            std::string result;
            result.reserve(_buffer.available() * 2);
            const unsigned char* p = up::char_cast<unsigned char>(_buffer.warm());
            const unsigned char* q = p + _buffer.available();
            for (; p != q; ++p) {
                result.push_back(chars[*p / _16]);
                result.push_back(chars[*p % _16]);
            }
            return result;
        }
        friend bool operator==(const self& lhs, const self& rhs)
        {
            return std::equal(
                lhs._buffer.warm(), lhs._buffer.warm() + lhs._buffer.available(),
                rhs._buffer.warm(), rhs._buffer.warm() + rhs._buffer.available());
        }
        friend bool operator!=(const self& lhs, const self& rhs)
        {
            return !(lhs == rhs);
        }
    };

    auto compress(const char* text)
    {
        return bytes::raw(up::zlib::compress(up::chunk::from(std::string(text))));
    }

    UP_TEST_CASE {
        UP_TEST_EQUAL(compress(""), bytes::hex("789c030000000001"));
        UP_TEST_EQUAL(compress("f"), bytes::hex("789c4b030000670067"));
        UP_TEST_EQUAL(compress("fo"), bytes::hex("789c4bcb0700013d00d6"));
        UP_TEST_EQUAL(compress("foo"), bytes::hex("789c4bcbcf070002820145"));
        UP_TEST_EQUAL(compress("foob"), bytes::hex("789c4bcbcf4f0200042901a7"));
        UP_TEST_EQUAL(compress("fooba"), bytes::hex("789c4bcbcf4f4a040006310208"));
        UP_TEST_EQUAL(compress("foobar"), bytes::hex("789c4bcbcf4f4a2c020008ab027a"));
    };

}
