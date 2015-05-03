#pragma once

/**
 * A terse_map is similar to an unordered_map, but uses open addressing
 * instead of separate chaining. The primary design goal is a terse memory
 * usage, in particular for small objects.
 *
 * It supports most operations of std::unordered_map, except for operations
 * which make no sense for open addressing, i.e. that would require a node
 * based data structure.
 */

#include "up_swap.hpp"
#include "up_utility.hpp"


namespace up_terse_map
{

    template <
        typename Key,
        typename T,
        typename Hash = std::hash<Key>,
        typename Pred = std::equal_to<Key>>
    class terse_map final
    {
    private: // --- scope ---
        using self = terse_map;
        enum class tag : uint8_t { overflow = 253, upper = 254, removed = 254, pristine = 255, };
        static auto skip_distance(const tag* from)
        {
            const tag* to = from;
            while (*to == tag::removed || *to == tag::pristine) {
                ++to;
            }
            return std::distance(from, to);
        }
    public:
        using key_type = Key;
        using value_type = std::pair<const Key, T>;
        using mapped_type = T;
        using hasher = Hash;
        using key_equal = Pred;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        class iterator final
        {
        public: // --- scope ---
            using iterator_category = std::forward_iterator_tag;
            using value_type = terse_map::value_type;
            using difference_type = terse_map::difference_type;
            using pointer = terse_map::pointer;
            using reference = terse_map::reference;
        private:
            using self = iterator;
            friend terse_map;
        private: // --- state ---
            terse_map::tag* _tag;
            value_type* _value;
        public: // --- life ---
            explicit iterator(terse_map::tag* tag, value_type* value)
                : _tag(tag), _value(value)
            { }
        public: // --- operations ---
            void swap(self& rhs) noexcept
            {
                up::swap_noexcept(_tag, rhs._tag);
                up::swap_noexcept(_value, rhs._value);
            }
            friend void swap(self& lhs, self& rhs) noexcept
            {
                lhs.swap(rhs);
            }
            auto operator*() const -> reference
            {
                return *_value;
            }
            auto operator->() const -> pointer
            {
                return _value;
            }
            auto operator++() -> self&
            {
                auto offset = skip_distance(_tag + 1) + 1;
                std::advance(_tag, offset);
                std::advance(_value, offset);
                return *this;
            }
            auto operator++(int) -> self
            {
                auto offset = skip_distance(_tag + 1) + 1;
                return self(
                    std::exchange(_tag, std::next(_tag, offset)),
                    std::exchange(_value, std::next(_value, offset)));
                }
            friend bool operator==(const self& lhs, const self& rhs)
            {
                return lhs._tag == rhs._tag && lhs._value == rhs._value;
            }
            friend bool operator!=(const self& lhs, const self& rhs)
            {
                return !(lhs == rhs);
            }
        };
        class const_iterator final
        {
        public: // --- scope ---
            using iterator_category = std::forward_iterator_tag;
            using value_type = terse_map::value_type;
            using difference_type = terse_map::difference_type;
            using pointer = terse_map::const_pointer;
            using reference = terse_map::const_reference;
        private:
            using self = const_iterator;
            friend terse_map;
        private: // --- state ---
            const terse_map::tag* _tag;
            const value_type* _value;
        public: // --- life ---
            explicit const_iterator(const terse_map::tag* tag, const value_type* value)
                : _tag(tag), _value(value)
            { }
            const_iterator(iterator rhs)
                : _tag(rhs._tag), _value(rhs._value)
            { }
        public: // --- operations ---
            void swap(self& rhs) noexcept
            {
                up::swap_noexcept(_tag, rhs._tag);
                up::swap_noexcept(_value, rhs._value);
            }
            friend void swap(self& lhs, self& rhs) noexcept
            {
                lhs.swap(rhs);
            }
            auto operator*() const -> reference
            {
                return *_value;
            }
            auto operator->() const -> pointer
            {
                return _value;
            }
            auto operator++() -> self&
            {
                auto offset = skip_distance(_tag + 1) + 1;
                std::advance(_tag, offset);
                std::advance(_value, offset);
                return *this;
            }
            auto operator++(int) -> self
            {
                auto offset = skip_distance(_tag + 1) + 1;
                return self(
                    std::exchange(_tag, std::next(_tag, offset)),
                    std::exchange(_value, std::next(_value, offset)));
            }
            friend bool operator==(const self& lhs, const self& rhs)
            {
                return lhs._tag == rhs._tag && lhs._value == rhs._value;
            }
            friend bool operator!=(const self& lhs, const self& rhs)
            {
                return !(lhs == rhs);
            }
        };
    private: // --- state ---
        size_type _capacity = 0;
        size_type _size = 0;
        size_type _removed = 0;
        std::unique_ptr<char[]> _raw = {};
        hasher _hasher = {};
        key_equal _equal = {};
        float _max_load_factor = 0.8;
    public: // --- life ---
        terse_map() noexcept = default;
        explicit terse_map(
            size_type capacity,
            const hasher& hash = hasher(),
            const key_equal& equal = key_equal())
            : _hasher(hash), _equal(equal)
        {
            if (capacity) {
                capacity += (_aligned(_tag_size(capacity)) - _tag_size(capacity)) / sizeof(tag);
                _raw = std::make_unique<char[]>(_aligned(_tag_size(capacity)) + _capacity * sizeof(value_type));
                std::uninitialized_fill_n(_tags(), _capacity, tag::pristine);
                new (_tags() + _capacity) tag(tag::overflow);
                _capacity = capacity;
            }
        }
        template <typename InputIterator>
        terse_map(InputIterator first, InputIterator last)
            : terse_map()
        {
            insert(first, last);
        }
        template <typename InputIterator>
        terse_map(
            InputIterator first, InputIterator last,
            size_type capacity,
            const hasher& hash = hasher(),
            const key_equal& equal = key_equal())
            : terse_map(capacity, hash, equal)
        {
            insert(first, last);
        }
        terse_map(const self& rhs)
            : terse_map(rhs._capacity, rhs._hasher, rhs._equal)
        {
            _max_load_factor = rhs._max_load_factor;
            insert(rhs.begin(), rhs.end());
        }
        terse_map(self&& rhs) noexcept
            : terse_map()
        {
            swap(rhs);
        }
        terse_map(std::initializer_list<value_type> init)
            : terse_map(init.begin(), init.end())
        { }
        terse_map(
            std::initializer_list<value_type> init,
            size_type capacity,
            const hasher& hash = hasher(),
            const key_equal& equal = key_equal())
            : terse_map(init.begin(), init.end(), capacity, hash, equal)
        { }
        ~terse_map() noexcept
        {
            clear();
            if (_raw) {
                for (size_type i = 0; i <= _capacity; ++i) {
                    _tags()[i].~tag();
                }
            }
        }
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self&
        {
            self(rhs).swap(*this);
            return *this;
        }
        auto operator=(self&& rhs) & noexcept -> self&
        {
            swap(rhs);
            return *this;
        }
        auto operator=(std::initializer_list<value_type> init) & -> self&
        {
            self temp;
            temp._max_load_factor = _max_load_factor;
            temp.insert(init.begin(), init.end());
            swap(temp);
            return *this;
        }

        // size and capacity
        bool empty() const noexcept
        {
            return _size == 0;
        }
        auto size() const noexcept -> size_type
        {
            return _size;
        }
        auto max_size() const noexcept -> size_type
        {
            return ~size_type(0) / (sizeof(value_type) + sizeof(tag));
        }

        // iterators
        auto begin() noexcept -> iterator
        {
            if (_raw) {
                auto offset = skip_distance(_tags());
                return iterator(_tags() + offset, _values() + offset);
            } else {
                return end();
            }
        }
        auto begin() const noexcept -> const_iterator
        {
            return cbegin();
        }
        auto end() noexcept -> iterator
        {
            if (_raw) {
                return iterator(_tags() + _capacity, _values() + _capacity);
            } else {
                return iterator(nullptr, nullptr);
            }
        }
        auto end() const noexcept -> const_iterator
        {
            return cend();
        }
        auto cbegin() const noexcept -> const_iterator
        {
            if (_raw) {
                auto offset = skip_distance(_tags());
                return const_iterator(_tags() + offset, _values() + offset);
            } else {
                return end();
            }
        }
        auto cend() const noexcept -> const_iterator
        {
            if (_raw) {
                return const_iterator(_tags() + _capacity, _values() + _capacity);
            } else {
                return const_iterator(nullptr, nullptr);
            }
        }

        // modifiers
        template <typename... Args>
        auto emplace(Args&&... args) -> std::pair<iterator, bool>
        {
            return _insert_value(std::forward<Args>(args)...);
        }
        auto insert(const value_type& obj) -> std::pair<iterator, bool>
        {
            return _insert_value(obj);
        }
        template <
            typename P,
            typename = typename std::enable_if<std::is_constructible<value_type, P>::value>::type>
        auto insert(P&& obj) -> std::pair<iterator, bool>
        {
            return _insert_value(std::forward<P>(obj));
        }
        template <typename InputIterator>
        void insert(InputIterator first, InputIterator last)
        {
            for (; first != last; ++first) {
                _insert_value(*first);
            }
        }
        void insert(std::initializer_list<value_type> init)
        {
            insert(init.begin(), init.end());
        }
        auto erase(const_iterator position) -> iterator
        {
            auto tags = _tags();
            auto values = _values();
            auto index = position._tag - tags;
            --_size;
            ++_removed;
            values[index].~value_type();
            tags[index] = tag::removed;
            index += skip_distance(tags + index + 1) + 1;
            return iterator(tags + index, values + index);
        }
        auto erase(const key_type& key) -> size_type
        {
            auto p = find(key);
            if (p == end()) {
                return 0;
            } else {
                erase(p);
                return 1;
            }
        }
        auto erase(const_iterator first, const_iterator last) -> iterator
        {
            while (first != last) {
                first = erase(first);
            }
            auto tags = _tags();
            auto index = first._tag - tags;
            return iterator(tags + index, _values() + index);
        }
        void clear() noexcept
        {
            erase(begin(), end());
        }
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_capacity, rhs._capacity);
            up::swap_noexcept(_size, rhs._size);
            up::swap_noexcept(_removed, rhs._removed);
            up::swap_noexcept(_raw, rhs._raw);
            up::swap_noexcept(_hasher, rhs._hasher);
            up::swap_noexcept(_equal, rhs._equal);
            up::swap_noexcept(_max_load_factor, rhs._max_load_factor);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }

        // observers
        auto hash_function() const -> hasher
        {
            return _hasher;
        }
        auto key_eq() const -> key_equal
        {
            return _equal;
        }

        // lookup
        auto find(const key_type& key) -> iterator
        {
            auto index = _find_aux(_hasher(key), key).first;
            return iterator(_tags() + index, _values() + index);
        }
        auto find(const key_type& key) const -> const_iterator
        {
            auto index = _find_aux(_hasher(key), key).first;
            return const_iterator(_tags() + index, _values() + index);
        }
        auto count(const key_type& key) const -> size_type
        {
            auto p = find(key);
            return p == end() ? 0 : 1;
        }
        auto equal_range(const key_type& key) -> std::pair<iterator, iterator>
        {
            auto p = find(key);
            if (p == end()) {
                return { p, p };
            } else {
                return { p, std::next(p) };
            }
        }
        auto equal_range(const key_type& key) const -> std::pair<const_iterator, const_iterator>
        {
            auto p = find(key);
            if (p == end()) {
                return { p, p };
            } else {
                return { p, std::next(p) };
            }
        }
        auto operator[](const key_type& key) -> mapped_type&
        {
            auto hash = _hasher(key);
            auto index = _find_aux(hash, key).first;
            if (index == _capacity) {
                return _insert_decomposed(key, mapped_type()).first->second;
            } else {
                return _values()[index].second;
            }
        }
        auto operator[](key_type&& key) -> mapped_type&
        {
            auto hash = _hasher(key);
            auto index = _find_aux(hash, key).first;
            if (index == _capacity) {
                return _insert_decomposed(std::move(key), mapped_type()).first->second;
            } else {
                return _values()[index].second;
            }
        }
        auto at(const key_type& key) -> mapped_type&
        {
            auto p = find(key);
            if (p == end()) {
                throw std::out_of_range("up::terse_map::at: key not found");
            } else {
                return p->second;
            }
        }
        auto at(const key_type& key) const -> const mapped_type&
        {
            auto p = find(key);
            if (p == end()) {
                throw std::out_of_range("up::terse_map::at: key not found");
            } else {
                return p->second;
            }
        }

        // hash policy
        float load_factor() const noexcept
        {
            return _capacity == 0 ? 0.0f : float(_size + _removed) / _capacity;
        }
        float max_load_factor() const noexcept
        {
            return _max_load_factor;
        }
        void max_load_factor(float z)
        {
            constexpr float min = 0.1;
            constexpr float max = 0.9;
            if (z < min) {
                _max_load_factor = min;
            } else if (z > max) {
                _max_load_factor = max;
            } else {
                _max_load_factor = z;
            }
        }
        void rehash(size_type capacity)
        {
            if (capacity == 0 && _size == 0) {
                _capacity = 0;
                _raw.reset(nullptr);
            } else {
                capacity = std::max(
                    std::max(static_cast<size_type>(_size / _max_load_factor + 1), capacity),
                    std::max(_capacity + _capacity / 2, size_type(7)));
                /* The following implementation is inefficient, because it
                 * makes no use of move operations, neither for the key nor
                 * the mapped value. */
                terse_map temp(capacity, _hasher, _equal);
                temp._max_load_factor = _max_load_factor;
                temp.insert(begin(), end());
                swap(temp);
            }
        }
        void reserve(size_type capacity)
        {
            rehash(capacity == 0 ? capacity : capacity / _max_load_factor + 1);
        }

        friend bool operator==(const self& lhs, const self& rhs)
        {
            if (lhs._size == rhs._size) {
                for (auto i = lhs.begin(), j = lhs.end(); i != j; ++i) {
                    if (!rhs.count(i->first)) {
                        return false;
                    }
                }
                return true;
            } else {
                return false;
            }
        }
        friend bool operator!=(const self& lhs, const self& rhs)
        {
            return !(lhs == rhs);
        }

    private:
        auto _aligned(size_type n) const
        {
            auto r = n % alignof(value_type);
            return r == 0 ? n : n - r + alignof(value_type);
        }
        auto _tag_size(size_type n) const
        {
            return (n + 1) * sizeof(tag);
        }
        auto _tags()
        {
            return _raw ? reinterpret_cast<tag*>(_raw.get()) : nullptr;
        }
        auto _tags() const
        {
            return _raw ? reinterpret_cast<const tag*>(_raw.get()) : nullptr;
        }
        auto _values()
        {
            return _raw
                ? reinterpret_cast<value_type*>(_raw.get() + _aligned(_tag_size(_capacity)))
                : nullptr;
        }
        auto _values() const
        {
            return _raw
                ? reinterpret_cast<const value_type*>(_raw.get() + _aligned(_tag_size(_capacity)))
                : nullptr;
        }
        auto _find_aux(size_type hash, const key_type& key) const -> std::pair<size_type, size_type>
        {
            auto tags = _tags();
            auto values = _values();
            auto quick = tag(hash % up::to_underlying_type(tag::upper));
            auto initial = hash % _capacity;
            using pair = std::pair<size_type, size_type>;
            size_type empty = _capacity;
            for (auto range : {pair(initial, _capacity), pair(0, initial)}) {
                for (size_type i = range.first; i != range.second; ++i) {
                    auto t = tags[i];
                    auto& v = values[i];
                    if (t == tag::pristine) {
                        return { _capacity, empty == _capacity ? i : empty };
                    } else if (t == quick && _hasher(v.first) == hash && _equal(v.first, key)) {
                        return { i, _capacity };
                    } else if (t != tag::removed) {
                        // continue
                    } else if (empty == _capacity) {
                        empty = i;
                    } // else: continue
                }
            }
            return { _capacity, empty };
        }
        template <typename M>
        auto _insert_decomposed(const key_type& key, M&& mapped) -> std::pair<iterator, bool>
        {
            return _insert_final(key, std::forward<M>(mapped));
        }
        template <typename M>
        auto _insert_decomposed(key_type&& key, M&& mapped) -> std::pair<iterator, bool>
        {
            return _insert_final(std::move(key), std::forward<M>(mapped));
        }
        template <typename K, typename M>
        auto _insert_final(K&& key, M&& mapped) -> std::pair<iterator, bool>
        {
            auto tags = _tags();
            auto values = _values();
            size_type hash = _hasher(key);
            auto index = _find_aux(hash, key);
            if (index.first != _capacity) {
                return {iterator(tags + index.first, values + index.first), false};
            } else if (tags[index.second] == tag::removed) {
                new (values + index.second) value_type(
                    std::forward<K>(key), std::forward<M>(mapped));
                tags[index.second] = tag(hash % up::to_underlying_type(tag::upper));
                ++_size;
                --_removed;
                return {iterator(tags + index.second, values + index.second), true};
            } else if (_size + _removed < _capacity * _max_load_factor) {
                new (values + index.second) value_type(
                    std::forward<K>(key), std::forward<M>(mapped));
                tags[index.second] = tag(hash % up::to_underlying_type(tag::upper));
                ++_size;
                return {iterator(tags + index.second, values + index.second), true};
            } else {
                rehash((_size + 1) / _max_load_factor);
                return _insert_final(std::forward<K>(key), std::forward<M>(mapped));
            }
        }
        auto _insert_value(const value_type& obj) -> std::pair<iterator, bool>
        {
            return _insert_decomposed(obj.first, obj.second);
        }
        template <typename K, typename M>
        auto _insert_value(std::pair<K, M>&& obj) -> std::pair<iterator, bool>
        {
            return _insert_decomposed(std::forward<K>(obj.first), std::forward<M>(obj.second));
        }
        template <typename... Args>
        auto _insert_value(Args&&... args) -> std::pair<iterator, bool>
        {
            value_type value(std::forward<Args>(args)...);
            return _insert_decomposed(value.first, std::move(value.second));
        }
    };

}


namespace up
{

    using up_terse_map::terse_map;

}
