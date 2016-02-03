#include "up_buffer.hpp"

#include <cstring>

#include "up_char_cast.hpp"
#include "up_exception.hpp"
#include "up_ints.hpp"


namespace
{

    struct runtime;
    struct out_of_range;

    using size_type = up_buffer::buffer::size_type;
    using sizes = up::ints::domain<size_type>;


    class header
    {
    public: // --- state ---
        size_type _size;
        size_type _warm_pos;
        size_type _cold_pos;
    public: // --- life ---
        explicit header()
            : _size(), _warm_pos(), _cold_pos()
        { }
        explicit header(size_type size, size_type warm_pos, size_type cold_pos)
            : _size(size), _warm_pos(warm_pos), _cold_pos(cold_pos)
        { }
    public: // --- operations ---
        auto to_fabric() const -> up::fabric
        {
            return up::fabric(typeid(*this), "buffer-header",
                up::invoke_to_fabric_with_fallback(_size),
                up::invoke_to_fabric_with_fallback(_warm_pos),
                up::invoke_to_fabric_with_fallback(_cold_pos));
        }
    };


    const header null_header;
    char null_data = 0;

    auto get_const_header(const char* core) -> const header&
    {
        return core ? *reinterpret_cast<const header*>(core) : null_header;
    }

    auto get_mutable_header(char* core) -> header&
    {
        if (core) {
            return *reinterpret_cast<header*>(core);
        } else {
            up::raise<runtime>("buffer-null-state");
        }
    }

    auto get_data(char* core) -> char*
    {
        return core ? core + sizeof(header) : &null_data;
    }

    auto core_size(const header& h)
    {
        return sizes::or_length_error<runtime>::add(sizeof(header), h._size);
    }

    auto core_reallocate(char* core, const header& h) -> char*
    {
        /* REALLOC might be significantly faster than a combination of
         * new/delete for large memory blocks, because REALLOC can remap the
         * addresses of whole page ranges. */
        if (char* temp = up::char_cast<char>(std::realloc(core, core_size(h)))) {
            new (temp) header(h);
            return temp;
        } else {
            up::raise<runtime>("buffer-out-of-memory", h);
        }
    }

    void core_move_to_front(char* core)
    {
        auto&& h = get_mutable_header(core);
        char* data = get_data(core);
        std::memmove(data, data + h._warm_pos, h._cold_pos - h._warm_pos);
        h._cold_pos -= h._warm_pos;
        h._warm_pos = 0;
    }

}


up_buffer::buffer::buffer(const char* data, size_type size)
    : buffer()
{
    if (size) {
        _core = core_reallocate(nullptr, header(size, 0, size));
        std::memcpy(get_data(_core), data, size);
    }
}

up_buffer::buffer::buffer(up::chunk::from chunk)
    : buffer(chunk.data(), chunk.size())
{ }

up_buffer::buffer::buffer(const self& rhs)
    : buffer()
{
    size_type size = rhs.available();
    if (size) {
        _core = core_reallocate(nullptr, header(size, 0, size));
        std::memcpy(get_data(_core), rhs.warm(), size);
    }
}

up_buffer::buffer::buffer(self&& rhs) noexcept
    : buffer()
{
    swap(rhs);
}

up_buffer::buffer::~buffer() noexcept
{
    std::free(_core);
}

auto up_buffer::buffer::operator=(const self& rhs) & -> self&
{
    self(rhs).swap(*this);
    return *this;
}

auto up_buffer::buffer::operator=(self&& rhs) & noexcept -> self&
{
    swap(rhs);
    return *this;
}

auto up_buffer::buffer::warm() const noexcept -> const char*
{
    auto&& h = get_const_header(_core);
    return get_data(_core) + h._warm_pos;
}

auto up_buffer::buffer::warm() noexcept -> char*
{
    auto&& h = get_const_header(_core);
    return get_data(_core) + h._warm_pos;
}

auto up_buffer::buffer::available() const noexcept -> size_type
{
    auto&& h = get_const_header(_core);
    return h._cold_pos - h._warm_pos;
}

void up_buffer::buffer::consume(size_type n)
{
    if (_core) {
        auto&& h = get_mutable_header(_core);
        auto pos = sizes::or_range_error<runtime>::add(h._warm_pos, n);
        if (pos > h._cold_pos) {
            up::raise<out_of_range>("buffer-consume-overflow", h, n);
        } else {
            h._warm_pos = pos;
        }
    } else if (n) {
        up::raise<out_of_range>("buffer-consume-overflow", null_header, n);
    } // else: nothing
}

up_buffer::buffer::operator up::chunk::from() const
{
    return {warm(), available()};
}

auto up_buffer::buffer::cold() noexcept -> char*
{
    auto&& h = get_const_header(_core);
    return get_data(_core) + h._cold_pos;
}

auto up_buffer::buffer::capacity() const noexcept -> size_type
{
    auto&& h = get_const_header(_core);
    return h._size - h._cold_pos;
}

auto up_buffer::buffer::reserve(size_type required_cold_size) -> self&
{
    auto&& h = get_const_header(_core);
    auto bias_size = h._warm_pos;
    auto warm_size = h._cold_pos - h._warm_pos;
    auto cold_size = h._size - h._cold_pos;
    auto free_size = bias_size + cold_size;
    auto required_size = sizes::or_length_error<runtime>::add(warm_size, required_cold_size);
    if (_core == nullptr) {
        /* Initial allocation. */
        size_type size = std::max(required_cold_size, size_type(32));
        _core = core_reallocate(nullptr, header(size, 0, 0));
    } else if (warm_size && cold_size >= required_cold_size) {
        /* Nothing to do, since there is sufficient space available. We do not
         * even move-to-front, because the warm area is non-empty and might be
         * consumed before we run out of space. */
    } else if (free_size >= required_cold_size && free_size >= warm_size) {
        /* By moving a fairly small amount of data (at most 50%), we can
         * create sufficient space. */
        core_move_to_front(_core);
    } else if (free_size + warm_size < size_type(1 << 16)
        || free_size >= warm_size
        || !sizes::is_valid::add(bias_size, required_size)) {
        /* In this case, it is unlikely that realloc has any benefits. So,
         * allocate new memory and move the data to the front. */
        size_type size = std::max(
            sizes::unsafe::sum(required_size, warm_size / 2, cold_size),
            required_size); // note: safe fallback in case of overflow
        auto core = core_reallocate(nullptr, header(size, 0, warm_size));
        std::memcpy(get_data(core), get_data(_core) + bias_size, warm_size);
        std::free(std::exchange(_core, core));
    } else {
        /* In this case, realloc might be signifanctly faster than allocating
         * new memory. The data is not moved to the front, because that might
         * make things worse. */
        size_type size = std::max(
            sizes::unsafe::sum(bias_size, required_size, warm_size / 2, cold_size),
            bias_size + required_size); // note: safe fallback (checked above)
        _core = core_reallocate(_core, header(size, bias_size, bias_size + warm_size));
    }
    return *this;
}

void up_buffer::buffer::produce(size_type n)
{
    if (_core) {
        auto&& h = get_mutable_header(_core);
        auto pos = sizes::or_range_error<runtime>::add(h._cold_pos, n);
        if (pos > h._size) {
            up::raise<out_of_range>("buffer-produce-overflow", h, n);
        } else {
            h._cold_pos = pos;
        }
    } else if (n) {
        up::raise<out_of_range>("buffer-produce-overflow", null_header, n);
    } // else: nothing
}

up_buffer::buffer::operator up::chunk::into()
{
    return {cold(), capacity()};
}
