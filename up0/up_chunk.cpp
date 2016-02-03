#include "up_chunk.hpp"

#include "up_exception.hpp"


namespace
{

    struct runtime { };

}


auto up_chunk::chunk::into::drain(std::size_t n) -> std::size_t
{
    if (n >= _size) {
        _data += _size;
        _size = 0;
        return n - std::exchange(_size, 0);
    } else {
        _data += n;
        _size -= n;
        return 0;
    }
}


auto up_chunk::chunk::into_bulk_t::count() const -> std::size_t
{
    std::size_t result = 0;
    auto chunks = _chunks();
    for (std::size_t i = 0, j = _count(); i != j; ++i) {
        if (chunks[i].size()) {
            ++result;
        }
    }
    return result;
}

auto up_chunk::chunk::into_bulk_t::total() const -> std::size_t
{
    std::size_t result = 0;
    auto chunks = _chunks();
    for (std::size_t i = 0, j = _count(); i != j; ++i) {
        result += chunks[i].size();
    }
    return result;
}

auto up_chunk::chunk::into_bulk_t::head() const -> const into&
{
    auto chunks = _chunks();
    for (std::size_t i = 0, j = _count(); i != j; ++i) {
        if (chunks[i].size()) {
            return chunks[i];
        } // else: next
    }
    up::raise<runtime>("bad-chunk", count(), total());
}


auto up_chunk::chunk::from::drain(std::size_t n) -> std::size_t
{
    if (n >= _size) {
        _data += _size;
        _size = 0;
        return n - std::exchange(_size, 0);
    } else {
        _data += n;
        _size -= n;
        return 0;
    }
}


auto up_chunk::chunk::from_bulk_t::count() const -> std::size_t
{
    std::size_t result = 0;
    auto chunks = _chunks();
    for (std::size_t i = 0, j = _count(); i != j; ++i) {
        if (chunks[i].size()) {
            ++result;
        }
    }
    return result;
}

auto up_chunk::chunk::from_bulk_t::total() const -> std::size_t
{
    std::size_t result = 0;
    auto chunks = _chunks();
    for (std::size_t i = 0, j = _count(); i != j; ++i) {
        result += chunks[i].size();
    }
    return result;
}

auto up_chunk::chunk::from_bulk_t::head() const -> const from&
{
    auto chunks = _chunks();
    for (std::size_t i = 0, j = _count(); i != j; ++i) {
        if (chunks[i].size()) {
            return chunks[i];
        } // else: next
    }
    up::raise<runtime>("bad-chunk", count(), total());
}
