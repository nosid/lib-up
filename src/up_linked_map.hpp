#pragma once

/**
 * A linked_map is a combination of an std::unordered_map (aka hash map) with
 * a std::list (aka linked list). This combination of containers is frequently
 * required and therefore implemented directly (instead of using boost
 * multi-index-container).
 *
 * The container provides basically all operations from std::unordered_map and
 * std::list. However, not all list operations are implemented in the expected
 * way (e.g. splice), and it is unclear, whether all these operations are
 * actually useful.
 */

#include "up_ints.hpp"
#include "up_swap.hpp"


namespace up_linked_map
{

    class list_node
    {
    private: // --- scope ---
        using self = list_node;
    public: // --- state ---
        self* _list_prev;
        self* _list_next;
    protected: // --- life ---
        explicit list_node(self* list_prev, self* list_next)
            : _list_prev(list_prev), _list_next(list_next)
        { }
        list_node(const self& rhs) = delete;
        list_node(self&& rhs) noexcept = delete;
        ~list_node() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
    };


    class list_root final : public list_node
    {
    public: // --- life ---
        using list_node::list_node;
        list_root()
            : list_node(this, this)
        { }
    public: // --- operations ---
        void swap(list_root& rhs) noexcept
        {
            up::swap_noexcept(_list_prev, rhs._list_prev);
            up::swap_noexcept(_list_next, rhs._list_next);
        }
    };


    class hash_node
    {
    private: // --- scope ---
        using self = hash_node;
    public: // --- state ---
        self* _hash_prev;
        self* _hash_next;
    protected: // --- life ---
        explicit hash_node(self* hash_prev, self* hash_next)
            : _hash_prev(hash_prev), _hash_next(hash_next)
        { }
        hash_node(const self& rhs) = delete;
        hash_node(self&& rhs) noexcept = delete;
        ~hash_node() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self& = delete;
    };


    class hash_root final : public hash_node
    {
    public: // --- life ---
        using hash_node::hash_node;
        hash_root()
            : hash_node(this, this)
        { }
    };


    template <
        typename Key,
        typename T,
        typename Hash = std::hash<Key>,
        typename Pred = std::equal_to<Key>>
    class linked_map final
    {
    public: // --- scope ---
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
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = linked_map::value_type;
            using difference_type = linked_map::difference_type;
            using pointer = linked_map::pointer;
            using reference = linked_map::reference;
        private:
            using self = iterator;
            friend linked_map;
        private: // --- state ---
            list_node* _node;
        public: // --- life ---
            explicit iterator(list_node* node)
                : _node(node)
            { }
        public: // --- operations ---
            void swap(self& rhs) noexcept
            {
                up::swap_noexcept(_node, rhs._node);
            }
            friend void swap(self& lhs, self& rhs) noexcept
            {
                lhs.swap(rhs);
            }
            auto operator*() const -> reference
            {
                return static_cast<node*>(_node)->_value;
            }
            auto operator->() const -> pointer
            {
                return &operator*();
            }
            auto operator++() -> self&
            {
                _node = _node->_list_next;
                return *this;
            }
            auto operator++(int) -> self
            {
                return self(std::exchange(_node, _node->_list_next));
            }
            auto operator--() -> self&
            {
                _node = _node->_list_prev;
                return *this;
            }
            auto operator--(int) -> self
            {
                return self(std::exchange(_node, _node->_list_prev));
            }
            friend bool operator==(const self& lhs, const self& rhs)
            {
                return lhs._node == rhs._node;
            }
            friend bool operator!=(const self& lhs, const self& rhs)
            {
                return !(lhs == rhs);
            }
        };
        class const_iterator final
        {
        public: // --- scope ---
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = linked_map::value_type;
            using difference_type = linked_map::difference_type;
            using pointer = linked_map::const_pointer;
            using reference = linked_map::const_reference;
        private:
            using self = const_iterator;
            friend linked_map;
        private: // --- state ---
            const list_node* _node;
        public: // --- life ---
            explicit const_iterator(const list_node* node)
                : _node(node)
            { }
            const_iterator(iterator rhs)
                : _node(rhs._node)
            { }
        public: // --- operations ---
            void swap(self& rhs) noexcept
            {
                up::swap_noexcept(_node, rhs._node);
            }
            friend void swap(self& lhs, self& rhs) noexcept
            {
                lhs.swap(rhs);
            }
            auto operator*() const -> reference
            {
                return static_cast<const node*>(_node)->_value;
            }
            auto operator->() const -> pointer
            {
                return &operator*();
            }
            auto operator++() -> self&
            {
                _node = _node->_list_next;
                return *this;
            }
            auto operator++(int) -> self
            {
                return self(std::exchange(_node, _node->_list_next));
            }
            auto operator--() -> self&
            {
                _node = _node->_list_prev;
                return *this;
            }
            auto operator--(int) -> self
            {
                return self(std::exchange(_node, _node->_list_prev));
            }
            friend bool operator==(const self& lhs, const self& rhs)
            {
                return lhs._node == rhs._node;
            }
            friend bool operator!=(const self& lhs, const self& rhs)
            {
                return !(lhs == rhs);
            }
        };
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    private:
        using self = linked_map;
        class node final : public list_node, public hash_node
        {
        public: // --- state ---
            size_type _hash;
            value_type _value;
        public: // --- life ---
            template <typename... Args>
            explicit node(size_type hash, Args&&... args)
                : list_node(nullptr, nullptr)
                , hash_node(nullptr, nullptr)
                , _hash(hash)
                , _value(std::forward<Args>(args)...)
            { }
        };
    private: // --- state ---
        size_type _bucket_count = 0;
        std::unique_ptr<hash_root[]> _buckets = {};
        hasher _hasher = {};
        key_equal _equal = {};
        list_root _list = {};
        size_type _size = 0;
        float _max_load_factor = 1.0;
    public: // --- life ---
        linked_map() = default;
        explicit linked_map(
            size_type bucket_count,
            const hasher& hash = hasher(),
            const key_equal& equal = key_equal())
            : _hasher(hash), _equal(equal)
        {
            if (bucket_count) {
                _buckets = std::make_unique<hash_root[]>(bucket_count);
                _bucket_count = bucket_count;
            }
        }
        template <typename InputIterator>
        linked_map(InputIterator first, InputIterator last)
            : linked_map()
        {
            insert(first, last);
        }
        template <typename InputIterator>
        linked_map(
            InputIterator first, InputIterator last,
            size_type bucket_count,
            const hasher& hash = hasher(),
            const key_equal& equal = key_equal())
            : linked_map(bucket_count, hash, equal)
        {
            insert(first, last);
        }
        linked_map(const self& rhs)
            : linked_map(rhs._bucket_count, rhs._hasher, rhs._equal)
        {
            _max_load_factor = rhs._max_load_factor;
            insert(rhs.begin(), rhs.end());
        }
        linked_map(self&& rhs) noexcept
            : linked_map()
        {
            swap(rhs);
        }
        linked_map(std::initializer_list<value_type> init)
            : linked_map(init.begin(), init.end())
        { }
        linked_map(
            std::initializer_list<value_type> init,
            size_type bucket_count,
            const hasher& hash = hasher(),
            const key_equal& equal = key_equal())
            : linked_map(init.begin(), init.end(), bucket_count, hash, equal)
        { }
        ~linked_map() noexcept
        {
            _clear_aux();
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
            return ~size_type(0) / sizeof(value_type);
        }

        // iterators
        auto begin() noexcept -> iterator
        {
            return iterator(_list._list_next);
        }
        auto begin() const noexcept -> const_iterator
        {
            return cbegin();
        }
        auto end() noexcept -> iterator
        {
            return iterator(&_list);
        }
        auto end() const noexcept -> const_iterator
        {
            return cend();
        }
        auto cbegin() const noexcept -> const_iterator
        {
            return const_iterator(_list._list_next);
        }
        auto cend() const noexcept -> const_iterator
        {
            return const_iterator(&_list);
        }

        // reverse iterators (for list compatibility)
        auto rbegin() noexcept -> reverse_iterator
        {
            return reverse_iterator(end());
        }
        auto rbegin() const noexcept -> const_reverse_iterator
        {
            return crbegin();
        }
        auto rend() noexcept -> reverse_iterator
        {
            return reverse_iterator(begin());
        }
        auto rend() const noexcept -> const_reverse_iterator
        {
            return crend();
        }
        auto crbegin() const noexcept -> const_reverse_iterator
        {
            return const_reverse_iterator(end());
        }
        auto crend() const noexcept -> const_reverse_iterator
        {
            return const_reverse_iterator(begin());
        }

        // element access (for list compatibility)
        auto front() -> reference
        {
            return static_cast<node*>(_list._list_next)->_value;
        }
        auto front() const -> const_reference
        {
            return static_cast<const node*>(_list._list_next)->_value;
        }
        auto back() -> reference
        {
            return static_cast<node*>(_list._list_prev)->_value;
        }
        auto back() const -> const_reference
        {
            return static_cast<const node*>(_list._list_prev)->_value;
        }

        // modifiers
        template <typename... Args>
        auto emplace(Args&&... args) -> std::pair<iterator, bool>
        {
            return _emplace_aux(&_list, std::forward<Args>(args)...);
        }
        auto insert(const value_type& obj) -> std::pair<iterator, bool>
        {
            return _insert_value(&_list, obj);
        }
        template <
            typename P,
            typename = typename std::enable_if<std::is_constructible<value_type, P>::value>::type>
        auto insert(P&& obj) -> std::pair<iterator, bool>
        {
            return _insert_value(&_list, std::forward<P>(obj));
        }
        template <typename InputIterator>
        void insert(InputIterator first, InputIterator last)
        {
            for (; first != last; ++first) {
                _emplace_aux(&_list, *first);
            }
        }
        void insert(std::initializer_list<value_type> init)
        {
            insert(init.begin(), init.end());
        }
        auto erase(const_iterator position) -> iterator
        {
            return iterator(_erase_node(position._node)->_list_next);
        }
        auto erase(const key_type& key) -> size_type
        {
            if (auto node = _find_node(_hasher(key), key)) {
                _erase_node(node);
                return 1;
            } else {
                return 0;
            }
        }
        auto erase(const_iterator first, const_iterator last) -> iterator
        {
            auto position = first._node;
            while (position != last._node) {
                position = _erase_node(position)->_list_next;
            }
        }
        void clear() noexcept
        {
            _clear_aux();
        }
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_bucket_count, rhs._bucket_count);
            up::swap_noexcept(_buckets, rhs._buckets);
            up::swap_noexcept(_hasher, rhs._hasher);
            up::swap_noexcept(_equal, rhs._equal);
            up::swap_noexcept(_list, rhs._list);
            up::swap_noexcept(_size, rhs._size);
            up::swap_noexcept(_max_load_factor, rhs._max_load_factor);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }

        // modifiers (for list compatibility)
        template <typename... Args>
        bool emplace_front(Args&&... args)
        {
            return _emplace_aux(_list._list_next, std::forward<Args>(args)...).second;
        }
        void pop_front()
        {
            _erase_node(_list._list_next);
        }
        template <typename... Args>
        bool emplace_back(Args&&... args)
        {
            return _emplace_aux(&_list, std::forward<Args>(args)...).second;
        }
        void pop_back()
        {
            _erase_node(_list._list_prev);
        }
        bool push_front(const value_type& obj)
        {
            return _insert_value(_list._list_next, obj).second;
        }
        template <
            typename P,
            typename = typename std::enable_if<std::is_constructible<value_type, P>::value>::type>
        bool push_front(P&& obj)
        {
            return _insert_value(_list._list_next, std::forward<P>(obj)).second;
        }
        bool push_back(const value_type& obj)
        {
            return _insert_value(&_list, obj).second;
        }
        template <
            typename P,
            typename = typename std::enable_if<std::is_constructible<value_type, P>::value>::type>
        bool push_back(P&& obj)
        {
            return _insert_value(&_list, std::forward<P>(obj)).second;
        }

        // list operations (for list compatibility)
        bool splice(const_iterator position, self& other)
        {
            if (&other == this) {
                return false; // or undefined behavior
            } else {
                bool result = true;
                for (auto i = other.begin(), j = other.end(); i != j;) {
                    auto n = static_cast<node*>(i++._node);
                    if (_find_node(n->_hash, n->_value.first)) {
                        result = false;
                    } else {
                        _splice_node(position._node, other, n);
                    }
                }
                return result;
            }
        }
        bool splice(const_iterator position, self&& other)
        {
            return splice(position, other);
        }
        bool splice(const_iterator position, self& other, const_iterator it)
        {
            return splice(position, other, it, std::next(it));
        }
        bool splice(const_iterator position, self&& other, const_iterator it)
        {
            return splice(position, other, it);
        }
        bool splice(const_iterator position, self& other, const_iterator first, const_iterator last)
        {
            if (&other == this) {
                while (first != last) {
                    _splice_node(position._node, first++._node);
                }
                return true;
            } else {
                bool result = true;
                while (first != last) {
                    auto n = static_cast<const node*>(first++._node);
                    if (_find_node(n->_hash, n->_value.first)) {
                        result = false;
                    } else {
                        _splice_node(position._node, other, n);
                    }
                }
                return result;
            }
        }
        bool splice(const_iterator position, self&& other, const_iterator first, const_iterator last)
        {
            return splice(position, other, first, last);
        }
        void remove(const value_type& obj)
        {
            for (auto i = begin(), j = end(); i != j;) {
                if (*i == obj) {
                    i = erase(i);
                } else {
                    ++i;
                }
            }
        }
        template <typename Predicate>
        void remove_if(Predicate&& pred)
        {
            for (auto i = begin(), j = end(); i != j;) {
                if (pred(*i)) {
                    i = erase(i);
                } else {
                    ++i;
                }
            }
        }
        void unique()
        {
            unique(std::equal_to<value_type>{});
        }
        template <typename Predicate>
        void unique(Predicate&& pred)
        {
            if (_size > 1) {
                for (auto p = begin(), q = end(), r = std::next(p); r != q;) {
                    if (pred(*p, *r)) {
                        r = erase(r);
                    } else {
                        ++p;
                        ++r;
                    }
                }
            }
        }
        bool merge(self& other)
        {
            return merge(other, std::less<value_type>{});
        }
        bool merge(self&& other)
        {
            return merge(other);
        }
        // This function will only be implemented if actually required.
        template <typename Less>
        bool merge(self& other, Less&& less) = delete;
        template <typename Less>
        bool merge(self&& other, Less&& less)
        {
            return merge(other, std::forward<Less>(less));
        }
        void sort()
        {
            sort(std::less<value_type>{});
        }
        // This function will only be implemented if actually required.
        template <typename Less>
        void sort(Less&& less) = delete;
        void reverse() noexcept
        {
            for (auto p = &_list; p != &_list; p = p->_list_prev) {
                up::swap_noexcept(p->_list_prev, p->_list_next);
            }
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
            list_node* node = _find_node(_hasher(key), key);
            return iterator(node ? node : &_list);
        }
        auto find(const key_type& key) const -> const_iterator
        {
            list_node* node = _find_node(_hasher(key), key);
            return const_iterator(node ? node : &_list);
        }
        auto count(const key_type& key) const -> size_type
        {
            return _find_node(_hasher(key), key) ? 1 : 0;
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
            size_type hash = _hasher(key);
            if (auto n = _find_node(hash, key)) {
                return n->_value.second;
            } else {
                return _put_node(&_list, std::make_unique<node>(hash, std::piecewise_construct,
                        std::forward_as_tuple(key), std::tuple<>{}))->second;
            }
        }
        auto operator[](key_type&& key) -> mapped_type&
        {
            size_type hash = _hasher(key);
            if (auto n = _find_node(hash, key)) {
                return n->_value.second;
            } else {
                return _put_node(&_list, std::make_unique<node>(hash, std::piecewise_construct,
                        std::forward_as_tuple(std::move(key)), std::tuple<>{}))->second;
            }
        }
        auto at(const key_type& key) -> mapped_type&
        {
            if (auto node = _find_node(_hasher(key), key)) {
                return node->_value.second;
            } else {
                throw std::out_of_range("up::linked_map::at: key not found");
            }
        }
        auto at(const key_type& key) const -> const mapped_type&
        {
            if (auto node = _find_node(_hasher(key), key)) {
                return node->_value.second;
            } else {
                throw std::out_of_range("up::linked_map::at: key not found");
            }
        }

        // hash policy
        float load_factor() const noexcept
        {
            return _bucket_count == 0 ? 0.0f : float(_size) / _bucket_count;
        }
        float max_load_factor() const noexcept
        {
            return _max_load_factor;
        }
        void max_load_factor(float z)
        {
            constexpr float min = 0.1f;
            constexpr float max = 10.0;
            if (z < min) {
                _max_load_factor = min;
            } else if (z > max) {
                _max_load_factor = max;
            } else {
                _max_load_factor = z;
            }
        }
        void rehash(size_type requested_bucket_count)
        {
            if (requested_bucket_count == 0 && _size == 0) {
                _bucket_count = 0;
                _buckets.reset(nullptr);
            } else {
                auto minimum = size_type(7);
                if (requested_bucket_count > _bucket_count) {
                    _do_rehash(std::max(requested_bucket_count, minimum));
                } else {
                    auto calculated = static_cast<size_type>(_size / _max_load_factor + 1);
                    auto n = std::max(requested_bucket_count, std::max(minimum, calculated));
                    if (n < _bucket_count) {
                        _do_rehash(n);
                    } // else: nothing
                }
            }
        }
        void reserve(size_type size)
        {
            rehash(size == 0 ? size : size / _max_load_factor + 1);
        }

        friend bool operator==(const self& lhs, const self& rhs)
        {
            return lhs._size == rhs._size && std::equal(lhs.begin(), lhs.end(), rhs.begin());
        }
        friend bool operator!=(const self& lhs, const self& rhs)
        {
            return !(lhs == rhs);
        }

    private:
        auto _find_node(size_type hash, const key_type& key) const -> node*
        {
            if (_bucket_count) {
                auto bucket = &_buckets[hash % _bucket_count];
                for (auto i = bucket->_hash_next; i != bucket; i = i->_hash_next) {
                    auto n = static_cast<node*>(i);
                    if (n->_hash == hash && _equal(n->_value.first, key)) {
                        return n;
                    }
                }
            }
            return nullptr;
        }
        void _clear_aux()
        {
            for (list_node* n = _list._list_next; _size; --_size) {
                delete static_cast<node*>(std::exchange(n, n->_list_next));
            }
            _list._list_prev = _list._list_next = &_list;
            for (size_type i = 0; i != _bucket_count; ++i) {
                auto& bucket = _buckets[i];
                bucket._hash_prev = bucket._hash_next = &bucket;
            }
        }
        template <typename K, typename M>
        auto _insert_final(list_node* position, K&& key, M&& mapped) -> std::pair<iterator, bool>
        {
            size_type hash = _hasher(key);
            if (auto n = _find_node(hash, key)) {
                return { iterator(n), false };
            } else {
                return { _put_node(position, std::make_unique<node>(
                            hash, std::forward<K>(key), std::forward<M>(mapped))), true };
            }
        }
        template <typename M>
        auto _insert_decomposed(list_node* position, const key_type& key, M&& mapped)
            -> std::pair<iterator, bool>
        {
            return _insert_final(position, key, std::forward<M>(mapped));
        }
        template <typename M>
        auto _insert_decomposed(list_node* position, key_type&& key, M&& mapped)
            -> std::pair<iterator, bool>
        {
            return _insert_final(position, std::move(key), std::forward<M>(mapped));
        }
        auto _insert_value(list_node* position, const value_type& obj) -> std::pair<iterator, bool>
        {
            return _insert_decomposed(position, obj.first, obj.second);
        }
        template <typename K, typename M>
        auto _insert_value(list_node* position, std::pair<K, M>&& obj)
        {
            return _insert_decomposed(position, std::forward<K>(obj.first), std::forward<M>(obj.second));
        }
        template <typename... Args>
        auto _emplace_aux(list_node* position, Args&&... args) -> std::pair<iterator, bool>
        {
            auto temp = std::make_unique<node>(0, std::forward<Args>(args)...);
            temp->_hash = _hasher(temp->_value.first);
            if (auto n = _find_node(temp->_hash, temp->_value.first)) {
                return { iterator(n), false };
            } else {
                return { _put_node(position, std::move(temp)), true };
            }
        }
        auto _put_node(list_node* position, std::unique_ptr<node> n) -> iterator
        {
            if (_bucket_count == 0 || _size >= _bucket_count * _max_load_factor) {
                using sizes = up::ints::domain<size_type>::or_length_error<struct runtime>;
                reserve(sizes::sum(_size, _size / 2, 1));
            }
            node* node = n.release();
            // link into linked list (before position)
            _list_link(position->_list_prev, node);
            _list_link(node, position);
            // link into hash chains (somewhere)
            auto bucket = &_buckets[node->_hash % _bucket_count];
            _hash_link(bucket->_hash_prev, node);
            _hash_link(node, bucket);
            ++_size;
            return iterator(node);
        }
        auto _erase_node(const list_node* position)
        {
            std::unique_ptr<node> n(static_cast<node*>(const_cast<list_node*>(position)));
            _list_link(n->_list_prev, n->_list_next);
            _hash_link(n->_hash_prev, n->_hash_next);
            --_size;
            return n;
        }
        void _splice_node(const list_node* position, const list_node* it)
        {
            if (position != it) {
                _list_link(it->_list_prev, it->_list_next);
                _list_link(position->_list_prev, const_cast<list_node*>(it));
                _list_link(const_cast<list_node*>(it), const_cast<list_node*>(position));
            } // else: nothing to do
        }
        void _splice_node(const list_node* position, self& other, const list_node* it)
        {
            _put_node(const_cast<list_node*>(position), other._erase_node(it));
        }
        void _list_link(list_node* lhs, list_node* rhs)
        {
            lhs->_list_next = rhs;
            rhs->_list_prev = lhs;
        }
        void _hash_link(hash_node* lhs, hash_node* rhs)
        {
            lhs->_hash_next = rhs;
            rhs->_hash_prev = lhs;
        }
        void _do_rehash(size_type bucket_count)
        {
            auto buckets = std::make_unique<hash_root[]>(bucket_count);
            for (size_type i = 0; i != _bucket_count; ++i) {
                auto source = &_buckets[i];
                for (auto i = source->_hash_next; i != source;) {
                    auto n = static_cast<node*>(i);
                    i = i->_hash_next;
                    auto target = &buckets[n->_hash % bucket_count];
                    _hash_link(target->_hash_prev, n);
                    _hash_link(n, target);
                }
            }
            up::swap_noexcept(bucket_count, _bucket_count);
            up::swap_noexcept(buckets, _buckets);
        }
    };

}


namespace up
{

    using up_linked_map::linked_map;

}
