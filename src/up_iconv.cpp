#include "up_iconv.hpp"

#include <mutex>

#include <iconv.h>

#include "up_buffer.hpp"
#include "up_exception.hpp"
#include "up_nts.hpp"
#include "up_terminate.hpp"


namespace
{

    struct runtime;


    /**
     * Performance: Creating and destroying a wrapper instance takes about 1us
     * on my Intel I7 Notebook. In comparison, transforming a short string
     * takes about half the time.
     *
     * Conclusion: Creating instances is sufficiently fast. It's not
     * worthwhile to build an additional cache above the iconv library as long
     * as it isn't really necessary.
     */
    class wrapper final
    {
    private: // --- scope ---
        using self = wrapper;
    private: // --- state ---
        iconv_t _iconv;
        bool _dirty = false;
    public: // --- life ---
        explicit wrapper(const up::string_view& to, const up::string_view& from)
            : _iconv(::iconv_open(up::nts(to), up::nts(from)))
        {
            if (_iconv == iconv_t(-1)) {
                UP_RAISE(runtime, "iconv-bad-encoding"_s, to, from, up::errno_info(errno));
            }
        }
        wrapper(const self& rhs) = delete;
        wrapper(self&& rhs) noexcept = delete;
        ~wrapper() noexcept
        {
            if (::iconv_close(_iconv) != 0) {
                UP_TERMINATE("iconv-bad-close"_s, up::errno_info(errno));
            }
        }
    public: // --- operations ---
        auto transform(const up::string_view& to, const up::string_view& from, up::string_view string)
        {
            constexpr auto error = std::size_t(-1);
            if (_dirty) {
                if (::iconv(_iconv, nullptr, nullptr, nullptr, nullptr) == error) {
                    UP_RAISE(runtime, "iconv-bad-reset"_s, to, from, up::errno_info(errno));
                }
            }
            _dirty = true;
            /* The error handling is a bit strange with iconv(3), because the
             * function makes progress (changes the input and output buffers)
             * and returns an error at the same time. The following code is
             * strict and checks for inconsistencies between the buffer
             * parameters and the return value. */
            auto buffer = up::buffer();
            auto from_data = const_cast<char*>(string.data());
            auto from_size = string.size();
            auto&& process = [&] {
                for (;;) {
                    /* Apparently, iconv requires at least 11 bytes for the output
                     * buffer, at least for some multibyte conversions. */
                    buffer.reserve(from_size / 3 + 12);
                    auto into_data = buffer.cold();
                    auto into_size = buffer.capacity();
                    auto rv = ::iconv(_iconv, &from_data, &from_size, &into_data, &into_size);
                    if (rv != error && from_size == 0) {
                        buffer.produce(buffer.capacity() - into_size);
                        break; // done
                    } else if (from_size == 0) {
                        UP_RAISE(runtime, "iconv-bad-conversion"_s, to, from);
                    } else if (errno != E2BIG) {
                        UP_RAISE(runtime, "iconv-bad-conversion"_s, to, from, up::errno_info(errno));
                    } else if (buffer.capacity() == into_size) {
                        // strange, apparently there was no progress
                        UP_RAISE(runtime, "iconv-bad-conversion"_s, to, from, into_size);
                    } else {
                        buffer.produce(buffer.capacity() - into_size);
                        // continue
                    }
                }
            };
            process();
            // add shift sequence to initial state (if required)
            from_data = nullptr;
            process();
            _dirty = false;
            return std::string(buffer.warm(), buffer.available());
        }
    };


    class base
    {
    private: // --- state ---
        std::string _to;
        std::string _from;
        wrapper _wrapper;
    public: // --- life ---
        explicit base(std::string&& to, std::string&& from)
            : _to(std::move(to))
            , _from(std::move(from))
            , _wrapper(_to, _from)
        { }
    protected:
        ~base() noexcept = default;
    public: // --- operations ---
        auto transform(up::string_view string)
        {
            return _wrapper.transform(_to, _from, string);
        }
    };

}


class up_iconv::unique_iconv::impl final : public base
{
public: // --- life ---
    using base::base;
};


void up_iconv::unique_iconv::destroy(impl* ptr)
{
    std::default_delete<impl>()(ptr);
}

up_iconv::unique_iconv::unique_iconv(std::string to, std::string from)
    : _impl(up::impl_make(std::move(to), std::move(from)))
{ }

auto up_iconv::unique_iconv::operator()(up::string_view string) -> std::string
{
    return _impl->transform(string);
}


class up_iconv::shared_iconv::impl final : public base
{
public: // --- state ---
    std::mutex _mutex;
public: // --- life ---
    using base::base;
};


void up_iconv::shared_iconv::destroy(impl* ptr)
{
    std::default_delete<impl>()(ptr);
}

up_iconv::shared_iconv::shared_iconv(std::string to, std::string from)
    : _impl(up::impl_make(std::move(to), std::move(from)))
{ }

auto up_iconv::shared_iconv::operator()(up::string_view string) const -> std::string
{
    std::unique_lock<std::mutex> lock(_impl->_mutex);
    return _impl->transform(string);
}
