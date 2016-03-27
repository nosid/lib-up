#pragma once

#include <atomic>
#include <cstring>
#include <memory>

#include "up_ints.hpp"

namespace up_string_repr
{

    using size_type = std::size_t;
    static_assert(std::is_same<size_type, up::string_view::size_type>::value, "type mismatch");

    using traits_type = std::char_traits<char>;
    static_assert(std::is_same<traits_type, up::string_view::traits_type>::value, "type mismatch");


    class string_repr final
    {
    public: // --- scope ---
        class storage;
        template <bool Unique>
        class storage_deleter;
        template <bool Unique>
        using storage_ptr = std::unique_ptr<storage, storage_deleter<Unique>>;
        template <bool Unique>
        static auto make_storage(size_type capacity, size_type size) -> storage_ptr<Unique>;
        template <bool Unique>
        static auto clone_storage(const storage& storage) -> storage_ptr<Unique>;
        template <bool Unique>
        class handle;
    private:
        using tag = unsigned char;
        static const constexpr tag tag_external = std::numeric_limits<tag>::max();
        static const constexpr size_type ptr_size = sizeof(storage*);
        static const constexpr size_type sso_size = ptr_size + ptr_size;
        static const constexpr size_type inline_size = sso_size - sizeof(tag);
        static const constexpr size_type dummy_size = sso_size - sizeof(tag) - ptr_size;
        union sso
        {
            struct internal
            {
                tag _tag;
                char _data[inline_size];
            } _internal;
            struct external
            {
                tag _tag;
                char _dummy[dummy_size];
                storage* _ptr;
            } _external;
        };
        static_assert(std::is_standard_layout<sso>::value, "requires standard layout");
        static_assert(sizeof(sso) == sso_size, "unexpected size mismatch");
        static_assert(sizeof(typename sso::internal) == sizeof(typename sso::external), "unexpected size mismatch");
        static_assert(offsetof(sso, _internal) == offsetof(sso, _external), "offset mismatch");
        static_assert(offsetof(typename sso::internal, _tag) == offsetof(typename sso::external, _tag), "offset mismatch");
        static_assert(offsetof(typename sso::internal, _data) == offsetof(typename sso::external, _dummy), "offset mismatch");
    };


    class string_repr::storage final
    {
    private: // --- scope ---
        using self = storage;
        static const constexpr size_type self_size = sizeof(size_type) * 3;
        friend string_repr;
    public:
        static auto max_size() -> size_type
        {
            return std::numeric_limits<size_type>::max() - self_size;
        }
    private: // --- state --
        mutable std::atomic<size_type> _n_refs;
        size_type _capacity;
        size_type _size;
    private: // --- life ---
        explicit storage(size_type n_refs, size_type capacity, size_type size) noexcept
            : _n_refs(n_refs), _capacity(capacity), _size(size)
        {
            static_assert(noexcept(decltype(_n_refs)(n_refs)), "requires noexcept contructible");
            static_assert(noexcept(decltype(_capacity)(capacity)), "requires noexcept contructible");
            static_assert(noexcept(decltype(_size)(size)), "requires noexcept contructible");
        }
        storage(const self& rhs) = delete;
        storage(self&& rhs) noexcept = delete;
        ~storage() noexcept = default;
    private: // --- operations ---
        auto operator=(const self& rhs) & -> self = delete;
        auto operator=(self&& rhs) & noexcept -> self = delete;
    public:
        void acquire() const noexcept
        {
            static_assert(noexcept(_n_refs.fetch_add(1)), "requires noexcept");
            _n_refs.fetch_add(1);
        }
        bool release() const noexcept
        {
            static_assert(noexcept(_n_refs.fetch_sub(1)), "requires noexcept");
            return _n_refs.fetch_sub(1) == 1;
        }
        bool unique() const noexcept
        {
            static_assert(noexcept(_n_refs.load()), "requires noexcept");
            return _n_refs.load() == 1;
        }
        auto capacity() const noexcept -> size_type
        {
            return _capacity;
        }
        auto size() const noexcept -> size_type
        {
            return _size;
        }
        void set_size(size_type size) noexcept
        {
            _size = size;
        }
        auto data() const noexcept -> const char*
        {
            static_assert(std::is_standard_layout<self>::value, "requires standard layout");
            static_assert(sizeof(self) == self_size, "size mismatch");
            return static_cast<const char*>(static_cast<const void*>(this)) + self_size;
        }
        auto data() noexcept -> char*
        {
            static_assert(std::is_standard_layout<self>::value, "requires standard layout");
            static_assert(sizeof(self) == self_size, "size mismatch");
            return static_cast<char*>(static_cast<void*>(this)) + self_size;
        }
    };


    template <bool Unique>
    class string_repr::storage_deleter final
    {
    public: // --- operations ---
        void operator()(storage* ptr) const
        {
            if (Unique || ptr->release()) {
                static_assert(std::is_standard_layout<storage>::value, "requires standard layout");
                size_type size = sizeof(storage) + ptr->_capacity; // no overflow check required
                ptr->~storage();
                ::operator delete(static_cast<void*>(ptr), size);
            } // else: nothing
        }
    };

    extern template class string_repr::storage_deleter<false>;
    extern template class string_repr::storage_deleter<true>;


    template <bool Unique>
    auto string_repr::make_storage(size_type capacity, size_type size) -> storage_ptr<Unique>
    {
        static_assert(std::is_standard_layout<storage>::value, "requires standard layout");
        using sizes = up::ints::domain<size_type>::or_length_error;
        void* raw = ::operator new(sizes::add(sizeof(storage), capacity));
        return [raw](auto&&... args) {
            static_assert(
                noexcept(storage(std::forward<decltype(args)>(args)...)), "requires noexcept");
            return storage_ptr<Unique>(new (raw) storage(std::forward<decltype(args)>(args)...));
        }(size_type(1), capacity, size);
    }

    extern template auto string_repr::make_storage<false>(size_type capacity, size_type size) -> storage_ptr<false>;
    extern template auto string_repr::make_storage<true>(size_type capacity, size_type size) -> storage_ptr<true>;


    template <bool Unique>
    auto string_repr::clone_storage(const storage& storage) -> storage_ptr<Unique>
    {
        auto size = storage.size();
        auto result = make_storage<Unique>(size, size);
        traits_type::copy(result->data(), storage.data(), size);
        return result;
    }

    extern template auto string_repr::clone_storage<false>(const storage& storage) -> storage_ptr<false>;
    extern template auto string_repr::clone_storage<true>(const storage& storage) -> storage_ptr<true>;


    template <bool Unique>
    class string_repr::handle final
    {
    private: // --- scope ---
        using self = handle;
        friend handle<!Unique>;
    public:
        static auto max_size() -> size_type
        {
            return storage::max_size();
        }
    private: // --- state ---
        sso _sso;
    public: // --- life ---
        handle() noexcept
        {
            std::memset(&_sso, 0, sso_size);
        }
        explicit handle(size_type capacity, size_type size)
        {
            if (capacity <= inline_size) {
                _sso._internal._tag = up::ints::caster(size);
            } else {
                static_assert(tag_external > inline_size, "consistency check");
                _sso._external._tag = tag_external;
                _sso._external._ptr = make_storage<Unique>(capacity, size).release();
            }
        }
        handle(const self& rhs)
        {
            std::memcpy(&_sso, &rhs._sso, sso_size);
            if (_sso._external._tag != tag_external) {
                // nothing
            } else if (Unique) {
                _sso._external._ptr = clone_storage<true>(*_sso._external._ptr).release();
            } else {
                _sso._external._ptr->acquire();
            }
        }
        handle(self&& rhs) noexcept
        {
            std::memcpy(&_sso, &rhs._sso, sso_size);
            std::memset(&rhs._sso, 0, sso_size);
        }
        ~handle() noexcept
        {
            if (_sso._external._tag == tag_external) {
                storage_ptr<Unique>(_sso._external._ptr);
            } // else: nothing
        }
        explicit handle(const handle<!Unique>& rhs)
        {
            std::memcpy(&_sso, &rhs._sso, sso_size);
            if (_sso._external._tag == tag_external) {
                _sso._external._ptr = clone_storage<true>(*_sso._external._ptr).release();
            } // else: nothing
        }
        explicit handle(handle<!Unique>&& rhs)
        {
            std::memcpy(&_sso, &rhs._sso, sso_size);
            std::memset(&rhs._sso, 0, sso_size);
            if (_sso._external._tag != tag_external) {
                // nothing
            } else if (!Unique) {
                // nothing
            } else if (_sso._external._ptr->unique()) {
                // nothing
            } else {
                _sso._external._ptr = clone_storage<true>(*storage_ptr<!Unique>(_sso._external._ptr)).release();
            }
        }
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self&
        {
            return operator=(self(rhs));
        }
        auto operator=(self&& rhs) & noexcept -> self&
        {
            if (_sso._external._tag == tag_external) {
                storage_ptr<Unique>(_sso._external._ptr);
            } // else: nothing
            std::memcpy(&_sso, &rhs._sso, sso_size);
            std::memset(&rhs._sso, 0, sso_size);
            return *this;
        }
        auto operator=(const handle<!Unique>& rhs) & -> self&
        {
            return operator=(handle<!Unique>(rhs));
        }
        auto operator=(handle<!Unique>&& rhs) & noexcept -> self&
        {
            if (_sso._external._tag == tag_external) {
                storage_ptr<Unique>(_sso._external._ptr);
            } // else: nothing
            std::memcpy(&_sso, &rhs._sso, sso_size);
            std::memset(&rhs._sso, 0, sso_size);
            return *this;
        }
        void swap(self& rhs) noexcept
        {
            if (this != &rhs) {
                sso temp;
                std::memcpy(&temp, &_sso, sso_size);
                std::memcpy(&_sso, &rhs._sso, sso_size);
                std::memcpy(&rhs._sso, &temp, sso_size);
            } // else: nothing
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto capacity() const noexcept -> size_type
        {
            if (_sso._external._tag == tag_external) {
                return _sso._external._ptr->capacity();
            } else {
                return inline_size;
            }
        }
        auto size() const noexcept -> size_type
        {
            if (_sso._external._tag == tag_external) {
                return _sso._external._ptr->size();
            } else {
                return _sso._internal._tag;
            }
        }
        void set_size(size_type size) noexcept
        {
            if (_sso._external._tag == tag_external) {
                _sso._external._ptr->set_size(size);
            } else {
                _sso._internal._tag = up::ints::caster(size);
            }
        }
        auto data() const noexcept -> const char*
        {
            if (_sso._external._tag == tag_external) {
                return _sso._external._ptr->data();
            } else {
                return _sso._internal._data;
            }
        }
        auto data() noexcept -> char*
        {
            if (_sso._external._tag == tag_external) {
                return _sso._external._ptr->data();
            } else {
                return _sso._internal._data;
            }
        }
    };

    extern template class string_repr::handle<false>;
    extern template class string_repr::handle<true>;

}


namespace up
{

    using string_repr = up_string_repr::string_repr;

}
