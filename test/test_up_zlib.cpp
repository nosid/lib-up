#include "up_char_cast.hpp"
#include "up_zlib.hpp"
#include "up_test.hpp"

namespace
{

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
            return self(up::buffer(text, std::strlen(text))); // XXX: instead convert from hex
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
        return bytes::raw(up::zlib::compress(std::string(text)));
    }

    UP_TEST_CASE {
        UP_TEST_EQUAL(compress("foo"), bytes::hex("789c4bcbcf070002820145"));
    };

}
