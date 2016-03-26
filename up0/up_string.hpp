#pragma once

#include "up_ints.hpp"
#include "up_string_view.hpp"
#include "up_swap.hpp"

namespace up_string
{

    class basic_string_types final
    {
    private: // --- scope ---
        template <typename Primary, typename Secondary>
        using match_t = std::enable_if_t<std::is_same<Primary, Secondary>::value, Primary>;

    public:
        template <typename Traits>
        using string_view = up::basic_string_view<typename Traits::char_type, Traits>;

        template <typename Traits>
        using value_type = typename Traits::char_type;
        template <typename Traits>
        using size_type = match_t<std::size_t, typename string_view<Traits>::size_type>;
        template <typename Traits>
        using difference_type = match_t<std::ptrdiff_t, typename string_view<Traits>::difference_type>;
        template <typename Traits>
        using const_reference = match_t<const value_type<Traits>&, typename string_view<Traits>::const_reference>;
        template <typename Traits>
        using reference = value_type<Traits>&;
        template <typename Traits>
        using const_pointer = match_t<const value_type<Traits>*, typename string_view<Traits>::const_pointer>;
        template <typename Traits>
        using pointer = value_type<Traits>*;
        template <typename Traits>
        using const_iterator = const_pointer<Traits>;
        template <typename Traits>
        using iterator = pointer<Traits>;
        template <typename Traits>
        using const_reverse_iterator = std::reverse_iterator<const_iterator<Traits>>;
        template <typename Traits>
        using reverse_iterator = std::reverse_iterator<iterator<Traits>>;
    };


    template <typename Traits>
    class traits_copy_fill final
    {
    private: // --- scope ---
        using value_type = basic_string_types::value_type<Traits>;
        using size_type = basic_string_types::size_type<Traits>;
    private: // --- state ---
        const value_type* _data;
        size_type _size;
    public: // --- life ---
        explicit traits_copy_fill(const value_type* data, size_type size)
            : _data(data), _size(size)
        { }
    public: // --- operations ---
        auto size() const -> size_type
        {
            return _size;
        }
        void apply(value_type* s) const
        {
            Traits::copy(s, _data, _size);
        }
    };

    extern template class traits_copy_fill<std::char_traits<char>>;


    template <typename Traits>
    class traits_assign_fill final
    {
    private: // --- scope ---
        using value_type = basic_string_types::value_type<Traits>;
        using size_type = basic_string_types::size_type<Traits>;
    private: // --- state ---
        size_type _size;
        value_type _value;
    public: // --- life ---
        explicit traits_assign_fill(size_type size, value_type value)
            : _size(size), _value(value)
        { }
    public: // --- operations ---
        auto size() const -> size_type
        {
            return _size;
        }
        void apply(value_type* s) const
        {
            Traits::assign(s, _size, _value);
        }
    };

    extern template class traits_assign_fill<std::char_traits<char>>;


    template <typename Traits>
    class traits_value_fill final
    {
    private: // --- scope ---
        using value_type = basic_string_types::value_type<Traits>;
        using size_type = basic_string_types::size_type<Traits>;
    private: // --- state ---
        value_type _value;
    public: // --- life ---
        explicit traits_value_fill(value_type value)
            : _value(value)
        { }
    public: // --- operations ---
        auto size() const -> size_type
        {
            return 1;
        }
        void apply(value_type* s) const
        {
            Traits::assign(*s, _value);
        }
    };

    extern template class traits_value_fill<std::char_traits<char>>;


    template <typename Iterator>
    using is_forward = std::is_convertible<
        typename std::iterator_traits<Iterator>::iterator_category, std::forward_iterator_tag>;

    template <typename Traits, typename Iterator, bool IsForward = is_forward<Iterator>::value>
    class traits_iterator_fill
    {
    private: // --- scope ---
        using value_type = basic_string_types::value_type<Traits>;
        using size_type = basic_string_types::size_type<Traits>;
    private: // --- state ---
        Iterator _first;
        Iterator _last;
        size_type _size;
    public: // --- life ---
        explicit traits_iterator_fill(Iterator first, Iterator last)
            : _first(first), _last(last), _size(std::distance(first, last))
        { }
    public: // --- operations ---
        auto size() const -> size_type
        {
            return _size;
        }
        void apply(value_type* s) const
        {
            for (Iterator i = _first; i != _last; ++i, ++s) {
                Traits::assign(*s, *i);
            }
        }
    };

    template <typename Traits>
    class traits_iterator_fill_base final
    {
    private: // --- scope ---
        using value_type = basic_string_types::value_type<Traits>;
        using size_type = basic_string_types::size_type<Traits>;
        static const constexpr size_type _prefix_size = sizeof(size_type) + sizeof(size_type);
        template <typename Iterator>
        static auto _copy(Iterator& first, Iterator& last, value_type* s, size_type n) -> size_type
        {
            size_type i = 0;
            for (; i < n && first != last; ++i, ++first) {
                Traits::assign(s[i], *first);
            }
            return i;
        }
    private: // --- state ---
        size_type _size = 0;
        std::unique_ptr<value_type[]> _suffix;
        value_type _prefix[_prefix_size];
    protected: // --- life ---
        template <typename Iterator>
        explicit traits_iterator_fill_base(Iterator first, Iterator last)
        {
            _size = _copy(first, last, _prefix, _prefix_size);
            if (first != last) {
                auto initial_size = _prefix_size + _prefix_size + _prefix_size;
                _suffix = std::make_unique<value_type[]>(initial_size);
                _size += _copy(first, last, _suffix.get(), initial_size);
                while (first != last) {
                    size_type n = _size - _prefix_size;
                    _resize_suffix(n, up::ints::domain<size_type>::or_length_error::add(_size + n));
                    _size += _copy(first, last, _suffix.get() + n, _size);
                }
            } // else: done
        }
    public: // --- operations ---
        auto size() const -> size_type
        {
            return _size;
        }
        void apply(value_type* s) const
        {
            if (_size > _prefix_size) {
                Traits::copy(s, _prefix, _prefix_size);
                Traits::copy(s, _suffix.get(), _size - _prefix_size);
            } else {
                Traits::copy(s, _prefix, _size);
            }
        }
    private:
        void _resize_suffix(size_type old_capacity, size_type new_capacity)
        {
            auto temp = std::make_unique<value_type[]>(new_capacity);
            Traits::copy(temp.get(), _suffix.get(), old_capacity);
            swap(temp, _suffix);
        }
    };

    extern template class traits_iterator_fill_base<std::char_traits<char>>;

    template <typename Traits, typename Iterator>
    class traits_iterator_fill<Traits, Iterator, false> final : public traits_iterator_fill_base<Traits>
    {
    private: // --- scope ---
        using base = traits_iterator_fill_base<Traits>;
    public: // --- life ---
        using base::base;
    };


    template <typename SizeType>
    class extent final
    {
    public: // --- scope ---
        using self = extent;
        using size_type = SizeType;
    private: // --- state ---
        size_type _capacity;
        size_type _size;
    public: // --- life ---
        explicit extent(size_type capacity, size_type size)
            : _capacity(capacity), _size(size)
        { }
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_capacity, rhs._capacity);
            up::swap_noexcept(_size, rhs._size);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto capacity() const { return _capacity; }
        auto size() const { return _size; }
    };


    template <typename Repr>
    class basic_string_base : protected Repr
    {
    private: // --- scope ---
        using self = basic_string_base;
        using base = Repr;

    public:
        using traits_type = typename Repr::traits_type;
        using value_type = basic_string_types::value_type<traits_type>;
        using size_type = basic_string_types::size_type<traits_type>;
        using difference_type = basic_string_types::difference_type<traits_type>;
        using const_reference = basic_string_types::const_reference<traits_type>;
        using const_pointer = basic_string_types::const_pointer<traits_type>;
        using const_iterator = basic_string_types::const_iterator<traits_type>;
        using const_reverse_iterator = basic_string_types::const_reverse_iterator<traits_type>;

        using string_view = basic_string_types::string_view<traits_type>;

        static const constexpr size_type npos = string_view::npos;

    protected:
        using sizes = typename up::ints::domain<size_type>;

        enum class fill_construct_t { };
        static const constexpr fill_construct_t fill_construct = {};

        using copy_fill = traits_copy_fill<traits_type>;
        using assign_fill = traits_assign_fill<traits_type>;
        template <typename Iterator>
        using iterator_fill = traits_iterator_fill<traits_type, Iterator>;

        static auto _make_extent(size_type baseline, size_type headroom, size_type request)
        {
            return extent<size_type>(
                std::max(request, sizes::unsafe::sum(baseline, baseline / 2, 32)),
                request - headroom);
        }

        static auto _make_fill(const up::string_view& s) -> copy_fill
        {
            return copy_fill(s.data(), s.size());
        }

        static void _apply_fills(value_type* ptr __attribute__((unused))) { }
        template <typename..., typename Head, typename... Tail>
        static void _apply_fills(value_type* ptr, Head&& head, Tail&&... tail)
        {
            size_type size = head.size();
            std::forward<Head>(head).apply(ptr);
            _apply_fills(ptr + size, std::forward<Tail>(tail)...);
        }

        static void _range_check(bool condition, up::source&& source)
        {
            if (!condition) {
                throw up::make_throwable<std::out_of_range>(std::move(source));
            }
        }
        static auto _const_iterator(const_pointer ptr, size_type pos) -> const_iterator
        {
            return const_iterator(ptr + pos);
        }

    public: // --- life ---
        basic_string_base() noexcept = default;
        basic_string_base(const self& rhs) = default;
        basic_string_base(self&& rhs) noexcept = default;
        ~basic_string_base() noexcept = default;

        basic_string_base(const string_view& s)
            : basic_string_base(s.data(), s.size())
        { }
        basic_string_base(const string_view& s, size_type pos, size_type n = npos)
            : basic_string_base(s.substr(pos, n))
        { }
        basic_string_base(const value_type* s)
            : basic_string_base(s, traits_type::length(s))
        { }
        basic_string_base(const value_type* s, size_type n)
            : basic_string_base(fill_construct, {}, {}, copy_fill(s, n))
        { }
        basic_string_base(size_type n, value_type c)
            : basic_string_base(fill_construct, {}, {}, assign_fill(n, c))
        { }
        template <typename InputIterator>
        basic_string_base(InputIterator first, InputIterator last)
            : basic_string_base(fill_construct, {}, {}, iterator_fill<InputIterator>(first, last))
        { }
        basic_string_base(std::initializer_list<value_type> chars)
            : basic_string_base(chars.begin(), chars.end())
        { }
    protected:
        template <typename..., typename... Fills>
        explicit basic_string_base(fill_construct_t, size_type baseline, size_type headroom, Fills&&... fills)
            : base(_make_extent(baseline, headroom, sizes::or_length_error::sum(headroom, fills.size()...)))
        {
            _apply_fills(Repr::data(), std::forward<Fills>(fills)...);
        }

    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = default;
        auto operator=(self&& rhs) & noexcept -> self& = default;

        auto begin() const noexcept -> const_iterator
        {
            return cbegin();
        }
        auto end() const noexcept -> const_iterator
        {
            return cend();
        }

        auto rbegin() const noexcept -> const_reverse_iterator
        {
            return crbegin();
        }
        auto rend() const noexcept -> const_reverse_iterator
        {
            return crend();
        }

        auto cbegin() const noexcept -> const_iterator
        {
            return _const_iterator(_data(), {});
        }
        auto cend() const noexcept -> const_iterator
        {
            return _const_iterator(_data(), _size());
        }
        auto crbegin() const noexcept -> const_reverse_iterator
        {
            return const_reverse_iterator(cend());
        }
        auto crend() const noexcept -> const_reverse_iterator
        {
            return const_reverse_iterator(cbegin());
        }

        auto size() const noexcept -> size_type
        {
            return _size();
        }
        auto length() const noexcept -> size_type
        {
            return _size();
        }
        auto max_size() const noexcept -> size_type
        {
            return Repr::max_size();
        }

        auto capacity() const noexcept -> size_type
        {
            return _capacity();
        }
        bool empty() const noexcept
        {
            return !_size();
        }

        auto operator[](size_type pos) const -> const_reference
        {
            return _data()[pos];
        }
        auto at(size_type pos) const -> const_reference
        {
            _range_check(pos < _size(), "up-string-bad-position");
            return _data()[pos];
        }

        auto front() const -> const_reference
        {
            return _data()[0];
        }
        auto back() const -> const_reference
        {
            return _data()[_size() - size_type(1)];
        }

        auto copy(value_type* s, size_type n, size_type pos = {}) const -> size_type
        {
            return operator string_view().copy(s, n, pos);
        }
        void swap(self& rhs) noexcept
        {
            Repr::swap(rhs);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }

        auto data() const noexcept -> const value_type*
        {
            return _data();
        }
        auto c_str() const noexcept -> const value_type* = delete; // intentionally not supported

        auto find(const string_view& s, size_type pos = {}) const noexcept -> size_type
        {
            return operator string_view().find(s, pos);
        }
        auto find(const value_type* s, size_type pos, size_type n) const noexcept -> size_type
        {
            return operator string_view().find(s, pos, n);
        }
        auto find(const value_type* s, size_type pos = {}) const noexcept -> size_type
        {
            return operator string_view().find(s, pos);
        }
        auto find(value_type c, size_type pos = {}) const noexcept -> size_type
        {
            return operator string_view().find(c, pos);
        }

        auto rfind(const string_view& s, size_type pos = npos) const noexcept -> size_type
        {
            return operator string_view().rfind(s, pos);
        }
        auto rfind(const value_type* s, size_type pos, size_type n) const noexcept -> size_type
        {
            return operator string_view().rfind(s, pos, n);
        }
        auto rfind(const value_type* s, size_type pos = npos) const noexcept -> size_type
        {
            return operator string_view().rfind(s, pos);
        }
        auto rfind(value_type c, size_type pos = npos) const noexcept -> size_type
        {
            return operator string_view().rfind(c, pos);
        }

        auto find_first_of(const string_view& s, size_type pos = {}) const noexcept -> size_type
        {
            return operator string_view().find_first_of(s, pos);
        }
        auto find_first_of(const value_type* s, size_type pos, size_type n) const noexcept -> size_type
        {
            return operator string_view().find_first_of(s, pos, n);
        }
        auto find_first_of(const value_type* s, size_type pos = {}) const noexcept -> size_type
        {
            return operator string_view().find_first_of(s, pos);
        }
        auto find_first_of(value_type c, size_type pos = {}) const noexcept -> size_type
        {
            return operator string_view().find_first_of(c, pos);
        }

        auto find_last_of(const string_view& s, size_type pos = npos) const noexcept -> size_type
        {
            return operator string_view().find_last_of(s, pos);
        }
        auto find_last_of(const value_type* s, size_type pos, size_type n) const noexcept -> size_type
        {
            return operator string_view().find_last_of(s, pos, n);
        }
        auto find_last_of(const value_type* s, size_type pos = npos) const noexcept -> size_type
        {
            return operator string_view().find_last_of(s, pos);
        }
        auto find_last_of(value_type c, size_type pos = npos) const noexcept -> size_type
        {
            return operator string_view().find_last_of(c, pos);
        }

        auto find_first_not_of(const string_view& s, size_type pos = {}) const noexcept -> size_type
        {
            return operator string_view().find_first_not_of(s, pos);
        }
        auto find_first_not_of(const value_type* s, size_type pos, size_type n) const noexcept -> size_type
        {
            return operator string_view().find_first_not_of(s, pos, n);
        }
        auto find_first_not_of(const value_type* s, size_type pos = {}) const noexcept -> size_type
        {
            return operator string_view().find_first_not_of(s, pos);
        }
        auto find_first_not_of(value_type c, size_type pos = {}) const noexcept -> size_type
        {
            return operator string_view().find_first_not_of(c, pos);
        }

        auto find_last_not_of(const string_view& s, size_type pos = npos) const noexcept -> size_type
        {
            return operator string_view().find_last_not_of(s, pos);
        }
        auto find_last_not_of(const value_type* s, size_type pos, size_type n) const noexcept -> size_type
        {
            return operator string_view().find_last_not_of(s, pos, n);
        }
        auto find_last_not_of(const value_type* s, size_type pos = npos) const noexcept -> size_type
        {
            return operator string_view().find_last_not_of(s, pos);
        }
        auto find_last_not_of(value_type c, size_type pos = npos) const noexcept -> size_type
        {
            return operator string_view().find_last_not_of(c, pos);
        }

        auto compare(const string_view& s) const noexcept -> int
        {
            return operator string_view().compare(s);
        }
        auto compare(size_type pos1, size_type n1, const string_view& s) const -> int
        {
            return operator string_view().compare(pos1, n1, s);
        }
        auto compare(size_type pos1, size_type n1, const string_view& s, size_type pos2, size_type n2) const -> int
        {
            return operator string_view().compare(pos1, n1, s, pos2, n2);
        }
        auto compare(const value_type* s) const noexcept -> int
        {
            return operator string_view().compare(s);
        }
        auto compare(size_type pos1, size_type n1, const value_type* s) const -> int
        {
            return operator string_view().compare(pos1, n1, s);
        }
        auto compare(size_type pos1, size_type n1, const value_type* s, size_type n2) const -> int
        {
            return operator string_view().compare(pos1, n1, s, n2);
        }

        operator string_view() const noexcept
        {
            return {_data(), _size()};
        }
        // XXX: temporary workaround for up::to_string
        auto to_string() const -> std::string
        {
            return {_data(), _size()};
        }
        // XXX: temporary workaround for up::out
        void out(std::ostream& os) const
        {
            os.write(_data(), _size());
        }

    protected:
        auto _size() const noexcept -> size_type
        {
            return Repr::size();
        }
        auto _capacity() const -> size_type
        {
            return Repr::capacity();
        }
        auto _data() const noexcept -> const_pointer
        {
            return Repr::data();
        }
    };


    template <typename Repr, bool Unique = false>
    class basic_string final : protected basic_string_base<Repr>
    {
    private: // --- scope ---
        using self = basic_string;
        using base = basic_string_base<Repr>;

    public:
        using traits_type = typename base::traits_type;
        using value_type = typename base::value_type;
        using size_type = typename base::size_type;
        using difference_type = typename base::difference_type;
        using const_reference = typename base::const_reference;
        using reference = typename base::const_reference;
        using const_pointer = typename base::const_pointer;
        using pointer = typename base::const_pointer;
        using const_iterator = typename base::const_iterator;
        using iterator = typename base::const_iterator;
        using const_reverse_iterator = typename base::const_reverse_iterator;
        using reverse_iterator = typename base::const_reverse_iterator;

        using string_view = typename base::string_view;

        static const constexpr auto npos = base::npos;
        static const constexpr auto fill_construct = base::fill_construct;

    private:
        using copy_fill = typename base::copy_fill;
        using base::_range_check;

    public:
        template <typename..., typename... Args>
        static auto concat(Args&&... args) -> self
        {
            return self(fill_construct, {}, {}, base::_make_fill(std::forward<Args>(args))...);
        }

    public: // --- life ---
        using base::base;

    public: // --- operations ---
        using base::begin;
        using base::end;
        using base::rbegin;
        using base::rend;
        using base::cbegin;
        using base::cend;
        using base::crbegin;
        using base::crend;

        using base::size;
        using base::length;
        using base::max_size;

        using base::capacity;
        using base::empty;

        using base::operator[];
        using base::at;

        using base::front;
        using base::back;

        using base::copy;
        using base::swap;
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }

        using base::data;
        using base::c_str;

        using base::find;
        using base::rfind;
        using base::find_first_of;
        using base::find_last_of;
        using base::find_first_not_of;
        using base::find_last_not_of;

        auto substr(size_type pos = {}, size_type n = npos) const -> self
        {
            auto size = _size();
            _range_check(pos <= size, "up-string-bad-position");
            return self(fill_construct, {}, {},
                copy_fill(_data() + pos, std::min(n, size - pos)));
        }

        using base::compare;
        using base::operator string_view;
        using base::to_string;
        using base::out;

    private:
        using base::_size;
        using base::_data;
    };


    template <typename Repr>
    class basic_string<Repr, true> final : protected basic_string_base<Repr>
    {
    private: // --- scope ---
        using self = basic_string;
        using base = basic_string_base<Repr>;

    public:
        using traits_type = typename base::traits_type;
        using value_type = typename base::value_type;
        using size_type = typename base::size_type;
        using difference_type = typename base::difference_type;
        using const_reference = typename base::const_reference;
        using reference = basic_string_types::reference<traits_type>;
        using const_pointer = typename base::const_pointer;
        using pointer = basic_string_types::pointer<traits_type>;
        using const_iterator = typename base::const_iterator;
        using iterator = basic_string_types::iterator<traits_type>;
        using const_reverse_iterator = typename base::const_reverse_iterator;
        using reverse_iterator = basic_string_types::reverse_iterator<traits_type>;

        using string_view = typename base::string_view;

        static const constexpr auto npos = base::npos;
        static const constexpr auto fill_construct = base::fill_construct;

    private:
        using copy_fill = traits_copy_fill<traits_type>;
        using assign_fill = traits_assign_fill<traits_type>;
        using value_fill = traits_value_fill<traits_type>;
        template <typename Iterator>
        using iterator_fill = traits_iterator_fill<traits_type, Iterator>;

        using base::_range_check;
        using base::_const_iterator;
        static auto _iterator(pointer ptr, size_type pos) -> iterator
        {
            return iterator(ptr + pos);
        }

    public:
        template <typename..., typename... Args>
        static auto concat(Args&&... args) -> self
        {
            return self(fill_construct, {}, {}, base::_make_fill(std::forward<Args>(args))...);
        }

    public: // --- life ---
        using base::base;

    public: // --- operations ---
        auto operator=(const string_view& s) -> self&
        {
            return assign(s);
        }
        auto operator=(const value_type* s) -> self&
        {
            return assign(s);
        }
        auto operator=(value_type c) -> self&
        {
            return assign(size_type(1), c);
        }
        auto operator=(std::initializer_list<value_type> chars) -> self&
        {
            return assign(chars.begin(), chars.end());
        }

        using base::begin;
        auto begin() noexcept -> iterator
        {
            return _iterator(_data(), {});
        }
        using base::end;
        auto end() noexcept -> iterator
        {
            return _iterator(_data(), _size());
        }
        using base::rbegin;
        auto rbegin() noexcept -> reverse_iterator
        {
            return reverse_iterator(end());
        }
        using base::rend;
        auto rend() noexcept -> reverse_iterator
        {
            return reverse_iterator(begin());
        }
        using base::cbegin;
        using base::cend;
        using base::crbegin;
        using base::crend;

        using base::size;
        using base::length;
        using base::max_size;

        void resize(size_type n, value_type c)
        {
            auto size = _size();
            if (n > size) {
                append(n - size, c);
            } else {
                _trim_size(n);
            }
        }
        void resize(size_type n)
        {
            resize(n, {});
        }
        using base::capacity;
        void reserve(size_type request = {})
        {
            auto capacity = _capacity();
            if (request > capacity) {
                auto size = _size();
                self(fill_construct, {}, request - size, copy_fill(_data(), size)).swap(*this);
            } else if (capacity - request < sizeof(self)) {
                // nothing
            } else if (capacity - _size() < sizeof(self)) {
                // nothing
            } else {
                auto size = _size();
                self(fill_construct, {}, request - size, copy_fill(_data(), size)).swap(*this);
            }
        }
        void shrink_to_fit()
        {
            if (_capacity() - _size() < sizeof(self)) {
                // nothing
            } else {
                self(*this).swap(*this);
            }
        }
        void clear() noexcept
        {
            _trim_size({});
        }
        using base::empty;

        using base::operator[];
        auto operator[](size_type pos) -> reference
        {
            return _data()[pos];
        }
        using base::at;
        auto at(size_type pos) -> reference
        {
            _range_check(pos < _size(), "up-string-bad-position");
            return _data()[pos];
        }

        using base::front;
        auto front() -> reference
        {
            return _data()[0];
        }
        using base::back;
        auto back() -> reference
        {
            return _data()[_size() - size_type(1)];
        }

        auto operator+=(const string_view& s) -> self&
        {
            return append(s);
        }
        auto operator+=(const value_type* s) -> self&
        {
            return append(s);
        }
        auto operator+=(value_type c) -> self&
        {
            return append(size_type(1), c);
        }
        auto operator+=(std::initializer_list<value_type> chars) -> self&
        {
            return append(chars);
        }

        auto append(const string_view& s) -> self&
        {
            return append(s.data(), s.size());
        }
        auto append(const string_view& s, size_type pos, size_type n) -> self&
        {
            return append(s.substr(pos, n));
        }
        auto append(const value_type* s, size_type n) -> self&
        {
            return _append_fill(copy_fill(s, n));
        }
        auto append(const value_type* s) -> self&
        {
            return append(s, traits_type::length(s));
        }
        auto append(size_type n, value_type c) -> self&
        {
            return _append_fill(assign_fill(n, c));
        }
        template <typename InputIterator>
        auto append(InputIterator first, InputIterator last) -> self&
        {
            return _append_fill(iterator_fill<InputIterator>(first, last));
        }
        auto append(std::initializer_list<value_type> chars) -> self&
        {
            return append(chars.begin(), chars.end());
        }

        void push_back(value_type c)
        {
            _append_fill(value_fill(c));
        }

        auto assign(const self& rhs) -> self&
        {
            return operator=(rhs);
        }
        auto assign(self&& rhs) noexcept -> self&
        {
            return operator=(std::forward<self>(rhs));
        }
        auto assign(const string_view& s) -> self&
        {
            return assign(s.data(), s.size());
        }
        auto assign(const string_view& s, size_type pos, size_type n) -> self&
        {
            return assign(s.substr(pos, n));
        }
        auto assign(const value_type* s, size_type n) -> self&
        {
            return _assign_fill(copy_fill(s, n));
        }
        auto assign(const value_type* s) -> self&
        {
            return assign(s, traits_type::length(s));
        }
        auto assign(size_type n, value_type c) -> self&
        {
            return _assign_fill(assign_fill(n, c));
        }
        template <typename InputIterator>
        auto assign(InputIterator first, InputIterator last) -> self&
        {
            return _assign_fill(iterator_fill<InputIterator>(first, last));
        }
        auto assign(std::initializer_list<value_type> chars) -> self&
        {
            return assign(chars.begin(), chars.end());
        }

        auto insert(size_type pos1, const string_view& s) -> self&
        {
            return insert(pos1, s.data(), s.size());
        }
        auto insert(size_type pos1, const string_view& s, size_type pos2, size_type n) -> self&
        {
            return insert(pos1, s.substr(pos2, n));
        }
        auto insert(size_type pos, const value_type* s, size_type n) -> self&
        {
            return _insert_fill(pos, copy_fill(s, n));
        }
        auto insert(size_type pos, const value_type* s) -> self&
        {
            return insert(pos, s, traits_type::length(s));
        }
        auto insert(size_type pos, size_type n, value_type c) -> self&
        {
            return _insert_fill(pos, assign_fill(n, c));
        }
        auto insert(const_iterator p, value_type c) -> iterator
        {
            return _insert_fill(p, value_fill(c));
        }
        auto insert(const_iterator p, size_type n, value_type c) -> iterator
        {
            return _insert_fill(p, assign_fill(n, c));
        }
        template <typename InputIterator>
        auto insert(const_iterator p, InputIterator first, InputIterator last) -> iterator
        {
            return _insert_fill(p, iterator_fill<InputIterator>(first, last));
        }
        auto insert(const_iterator p, std::initializer_list<value_type> chars) -> iterator
        {
            return insert(p, chars.begin(), chars.end());
        }

        auto erase(size_type pos = {}, size_type n = npos) -> self&
        {
            auto size = _size();
            _range_check(pos <= size, "up-string-bad-position");
            return _erase(pos, std::min(n, size - pos));
        }
        auto erase(const_iterator position) -> iterator
        {
            return erase(position, std::next(position));
        }
        auto erase(const_iterator first, const_iterator last) -> iterator
        {
            auto data = _data();
            size_type pos = std::distance(_const_iterator(data, {}), first);
            size_type n = std::distance(first, last);
            _erase(pos, n);
            return _iterator(data, pos);
        }

        void pop_back()
        {
            _trim_size(_size() - size_type(1));
        }

        auto replace(size_type pos1, size_type n1, const string_view& s) -> self&
        {
            return replace(pos1, n1, s.data(), s.size());
        }
        auto replace(size_type pos1, size_type n1, const string_view& s, size_type pos2, size_type n2) -> self&
        {
            return replace(pos1, n1, s.substr(pos2, n2));
        }
        auto replace(size_type pos, size_type n1, const value_type* s, size_type n2) -> self&
        {
            return _replace_fill(pos, n1, copy_fill(s, n2));
        }
        auto replace(size_type pos, size_type n1, const value_type* s) -> self&
        {
            return replace(pos, n1, s, traits_type::length(s));
        }
        auto replace(size_type pos, size_type n1, size_type n2, value_type c) -> self&
        {
            return _replace_fill(pos, n1, assign_fill(n2, c));
        }
        auto replace(const_iterator p, const_iterator q, const string_view& s) -> self&
        {
            return replace(p, q, s.data(), s.size());
        }
        auto replace(const_iterator p, const_iterator q, const value_type* s, size_type n) -> self&
        {
            return _replace_fill(p, q, copy_fill(s, n));
        }
        auto replace(const_iterator p, const_iterator q, const value_type* s) -> self&
        {
            return replace(p, q, s, traits_type::length(s));
        }
        auto replace(const_iterator p, const_iterator q, size_type n, value_type c) -> self&
        {
            return _replace_fill(p, q, assign_fill(n, c));
        }
        template <typename InputIterator>
        auto replace(const_iterator p, const_iterator q, InputIterator first, InputIterator last) -> self&
        {
            return _replace_fill(p, q, iterator_fill<InputIterator>(first, last));
        }
        auto replace(const_iterator p, const_iterator q, std::initializer_list<value_type> chars) -> self&
        {
            return replace(p, q, chars.begin(), chars.end());
        }

        using base::copy;
        using base::swap;
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }

        using base::data;
        using base::c_str;

        using base::find;
        using base::rfind;
        using base::find_first_of;
        using base::find_last_of;
        using base::find_first_not_of;
        using base::find_last_not_of;

        auto substr(size_type pos = {}, size_type n = npos) const -> self
        {
            auto size = _size();
            _range_check(pos <= size, "up-string-bad-position");
            return self(fill_construct, {}, {}, copy_fill(_data() + pos, std::min(n, size - pos)));
        }

        using base::compare;
        using base::operator string_view;
        using base::to_string;
        using base::out;

    private:
        using base::_size;
        using base::_capacity;
        using base::_data;
        auto _data() noexcept -> pointer
        {
            return Repr::data();
        }
        bool _increase_size(size_type n)
        {
            size_type size = base::sizes::or_length_error::add(_size(), n);
            if (size <= _capacity()) {
                Repr::set_size(size);
                return true;
            } else {
                return false;
            }
        }
        void _trim_size(size_type n)
        {
            Repr::set_size(n);
        }
        template <typename..., typename Fill>
        auto _append_fill(Fill&& fill) -> self&
        {
            size_type n = fill.size();
            if (_increase_size(n)) {
                fill.apply(_data() + _size() - n);
            } else {
                self(fill_construct, _capacity(), {},
                    copy_fill(_data(), _size()),
                    std::forward<Fill>(fill)).swap(*this);
            }
            return *this;
        }
        template <typename..., typename Fill>
        auto _assign_fill(Fill&& fill) -> self&
        {
            size_type n = fill.size();
            if (n <= _size()) {
                fill.apply(_data());
                _trim_size(n);
            } else if (_increase_size(n - _size())) {
                fill.apply(_data());
            } else {
                self(fill_construct, {}, {}, std::forward<Fill>(fill)).swap(*this);
            }
            return *this;
        }
        template <typename..., typename Fill>
        auto _insert_fill(size_type pos, Fill&& fill) -> self&
        {
            auto size = _size();
            auto data = _data();
            _range_check(pos <= size, "up-string-bad-position");
            size_type n = fill.size();
            if (_increase_size(n)) {
                traits_type::move(data + pos + n, data + pos, size - pos);
                fill.apply(data + pos);
            } else {
                self(fill_construct, _capacity(), {},
                    copy_fill(data, pos),
                    std::forward<Fill>(fill),
                    copy_fill(data + pos, size - pos)).swap(*this);
            }
            return *this;
        }
        template <typename..., typename Fill>
        auto _insert_fill(const_iterator p, Fill&& fill) -> iterator
        {
            size_type pos = std::distance(_const_iterator(_data(), {}), p);
            _insert_fill(pos, std::forward<Fill>(fill));
            return _iterator(_data(), pos);
        }
        template <typename..., typename Fill>
        auto _replace_fill(size_type pos, size_type n, Fill&& fill) -> self&
        {
            auto size = _size();
            auto data = _data();
            size_type k = fill.size();
            if (k <= n) {
                traits_type::move(data + pos + k, data + pos + n, size - pos - n);
                fill.apply(data + pos);
                _trim_size(size + (n - k));
            } else if (_increase_size(k - n)) {
                traits_type::move(data + pos + k, data + pos + n, size - pos - n);
                fill.apply(data + pos);
            } else {
                self(fill_construct, _capacity(), {},
                    copy_fill(data, pos),
                    std::forward<Fill>(fill),
                    copy_fill(data + pos + n, size - pos - n)).swap(*this);
            }
            return *this;
        }
        template <typename..., typename Fill>
        auto _replace_fill(const_iterator p, const_iterator q, Fill&& fill) -> self&
        {
            size_type pos = std::distance(_const_iterator(_data(), {}), p);
            size_type n = std::distance(p, q);
            return _replace_fill(pos, n, std::forward<Fill>(fill));
        }
        auto _erase(size_type pos, size_type n) -> self&
        {
            if (n) {
                auto size = _size();
                auto data = _data();
                traits_type::move(data + pos, data + pos + n, size - pos - n);
                _trim_size(size - n);
            } // else: nothing
            return *this;
        }
    };


    template <typename..., typename Repr, bool Unique>
    auto operator+(
        const basic_string<Repr, Unique>& lhs,
        const basic_string<Repr, Unique>& rhs)
        -> basic_string<Repr, Unique>
    {
        return basic_string<Repr, Unique>::concat(lhs, rhs);
    }

    template <typename..., typename Repr, bool Unique>
    auto operator+(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::string_view& rhs)
        -> basic_string<Repr, Unique>
    {
        return basic_string<Repr, Unique>::concat(lhs, rhs);
    }

    template <typename..., typename Repr, bool Unique>
    auto operator+(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::value_type* rhs)
        -> basic_string<Repr, Unique>
    {
        return basic_string<Repr, Unique>::concat(lhs, rhs);
    }

    template <typename..., typename Repr, bool Unique>
    auto operator+(
        const basic_string<Repr, Unique>& lhs,
        typename basic_string<Repr, Unique>::value_type rhs)
        -> basic_string<Repr, Unique>
    {
        return basic_string<Repr, Unique>::concat(lhs, rhs);
    }

    template <typename..., typename Repr, bool Unique>
    auto operator+(
        const typename basic_string<Repr, Unique>::string_view& lhs,
        const basic_string<Repr, Unique>& rhs)
        -> basic_string<Repr, Unique>
    {
        return basic_string<Repr, Unique>::concat(lhs, rhs);
    }

    template <typename..., typename Repr, bool Unique>
    auto operator+(
        const typename basic_string<Repr, Unique>::value_type* lhs,
        const basic_string<Repr, Unique>& rhs)
        -> basic_string<Repr, Unique>
    {
        return basic_string<Repr, Unique>::concat(lhs, rhs);
    }

    template <typename..., typename Repr, bool Unique>
    auto operator+(
        typename basic_string<Repr, Unique>::value_type lhs,
        const basic_string<Repr, Unique>& rhs)
        -> basic_string<Repr, Unique>
    {
        return basic_string<Repr, Unique>::concat(lhs, rhs);
    }


    template <typename..., typename Repr>
    auto operator+(
        basic_string<Repr, true>&& lhs,
        const basic_string<Repr, true>& rhs)
        -> basic_string<Repr, true>
    {
        return std::move(lhs.append(rhs));
    }

    template <typename..., typename Repr>
    auto operator+(
        const basic_string<Repr, true>& lhs,
        basic_string<Repr, true>&& rhs)
        -> basic_string<Repr, true>
    {
        return std::move(rhs.insert(0, lhs));
    }

    template <typename..., typename Repr>
    auto operator+(
        basic_string<Repr, true>&& lhs,
        basic_string<Repr, true>&& rhs)
        -> basic_string<Repr, true>
    {
        return std::move(lhs.append(rhs));
    }

    template <typename..., typename Repr>
    auto operator+(
        basic_string<Repr, true>&& lhs,
        const typename basic_string<Repr, true>::string_view& rhs)
        -> basic_string<Repr, true>
    {
        return std::move(lhs.append(rhs));
    }

    template <typename..., typename Repr>
    auto operator+(
        basic_string<Repr, true>&& lhs,
        const typename basic_string<Repr, true>::value_type* rhs)
        -> basic_string<Repr, true>
    {
        return std::move(lhs.append(rhs));
    }

    template <typename..., typename Repr>
    auto operator+(
        basic_string<Repr, true>&& lhs,
        typename basic_string<Repr, true>::value_type rhs)
        -> basic_string<Repr, true>
    {
        return std::move(lhs.append(&rhs, 1));
    }

    template <typename..., typename Repr>
    auto operator+(
        const typename basic_string<Repr, true>::string_view& lhs,
        basic_string<Repr, true>&& rhs)
        -> basic_string<Repr, true>
    {
        return std::move(rhs.insert(0, lhs));
    }

    template <typename..., typename Repr>
    auto operator+(
        const typename basic_string<Repr, true>::value_type* lhs,
        basic_string<Repr, true>&& rhs)
        -> basic_string<Repr, true>
    {
        return std::move(rhs.insert(0, lhs));
    }

    template <typename..., typename Repr>
    auto operator+(
        typename basic_string<Repr, true>::value_type lhs,
        basic_string<Repr, true>&& rhs)
        -> basic_string<Repr, true>
    {
        return std::move(rhs.insert(0, &lhs, 1));
    }


    template <typename..., typename Repr, bool Unique>
    bool operator==(
        const basic_string<Repr, Unique>& lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() == rhs.operator string_view();
    }

    template <typename..., typename Repr, bool Unique>
    bool operator==(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::string_view& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() == rhs;
    }

    template <typename..., typename Repr, bool Unique>
    bool operator==(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::value_type* rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() == rhs;
    }

    template <typename..., typename Repr, bool Unique>
    bool operator==(
        const typename basic_string<Repr, Unique>::string_view& lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs == rhs.operator string_view();
    }

    template <typename..., typename Repr, bool Unique>
    bool operator==(
        const typename basic_string<Repr, Unique>::value_type* lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs == rhs.operator string_view();
    }


    template <typename..., typename Repr, bool Unique>
    bool operator!=(
        const basic_string<Repr, Unique>& lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() != rhs.operator string_view();
    }

    template <typename..., typename Repr, bool Unique>
    bool operator!=(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::string_view& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() != rhs;
    }

    template <typename..., typename Repr, bool Unique>
    bool operator!=(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::value_type* rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() != rhs;
    }

    template <typename..., typename Repr, bool Unique>
    bool operator!=(
        const typename basic_string<Repr, Unique>::string_view& lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs != rhs.operator string_view();
    }

    template <typename..., typename Repr, bool Unique>
    bool operator!=(
        const typename basic_string<Repr, Unique>::value_type* lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs != rhs.operator string_view();
    }


    template <typename..., typename Repr, bool Unique>
    bool operator<(
        const basic_string<Repr, Unique>& lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() < rhs.operator string_view();
    }

    template <typename..., typename Repr, bool Unique>
    bool operator<(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::string_view& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() < rhs;
    }

    template <typename..., typename Repr, bool Unique>
    bool operator<(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::value_type* rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() < rhs;
    }

    template <typename..., typename Repr, bool Unique>
    bool operator<(
        const typename basic_string<Repr, Unique>::string_view& lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs < rhs.operator string_view();
    }

    template <typename..., typename Repr, bool Unique>
    bool operator<(
        const typename basic_string<Repr, Unique>::value_type* lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs < rhs.operator string_view();
    }


    template <typename..., typename Repr, bool Unique>
    bool operator>(
        const basic_string<Repr, Unique>& lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() > rhs.operator string_view();
    }

    template <typename..., typename Repr, bool Unique>
    bool operator>(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::string_view& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() > rhs;
    }

    template <typename..., typename Repr, bool Unique>
    bool operator>(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::value_type* rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() > rhs;
    }

    template <typename..., typename Repr, bool Unique>
    bool operator>(
        const typename basic_string<Repr, Unique>::string_view& lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs > rhs.operator string_view();
    }

    template <typename..., typename Repr, bool Unique>
    bool operator>(
        const typename basic_string<Repr, Unique>::value_type* lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs > rhs.operator string_view();
    }


    template <typename..., typename Repr, bool Unique>
    bool operator<=(
        const basic_string<Repr, Unique>& lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() <= rhs.operator string_view();
    }

    template <typename..., typename Repr, bool Unique>
    bool operator<=(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::string_view& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() <= rhs;
    }

    template <typename..., typename Repr, bool Unique>
    bool operator<=(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::value_type* rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() <= rhs;
    }

    template <typename..., typename Repr, bool Unique>
    bool operator<=(
        const typename basic_string<Repr, Unique>::string_view& lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs <= rhs.operator string_view();
    }

    template <typename..., typename Repr, bool Unique>
    bool operator<=(
        const typename basic_string<Repr, Unique>::value_type* lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs <= rhs.operator string_view();
    }


    template <typename..., typename Repr, bool Unique>
    bool operator>=(
        const basic_string<Repr, Unique>& lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() >= rhs.operator string_view();
    }

    template <typename..., typename Repr, bool Unique>
    bool operator>=(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::string_view& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() >= rhs;
    }

    template <typename..., typename Repr, bool Unique>
    bool operator>=(
        const basic_string<Repr, Unique>& lhs,
        const typename basic_string<Repr, Unique>::value_type* rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs.operator string_view() >= rhs;
    }

    template <typename..., typename Repr, bool Unique>
    bool operator>=(
        const typename basic_string<Repr, Unique>::string_view& lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs >= rhs.operator string_view();
    }

    template <typename..., typename Repr, bool Unique>
    bool operator>=(
        const typename basic_string<Repr, Unique>::value_type* lhs,
        const basic_string<Repr, Unique>& rhs) noexcept
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return lhs >= rhs.operator string_view();
    }


    template <typename..., typename Repr, bool Unique>
    auto operator<<(
        std::basic_ostream<typename Repr::traits_type::char_type, typename Repr::traits_type>& os,
        const basic_string<Repr, Unique>& s)
        -> std::basic_ostream<typename Repr::traits_type::char_type, typename Repr::traits_type>&
    {
        using string_view = typename basic_string<Repr, Unique>::string_view;
        return os << s.operator string_view();
    }


    class repr
    {
    private: // --- scope ---
        using self = repr;
    protected:
        using traits_type = std::char_traits<char>;
        using size_type = basic_string_types::size_type<traits_type>;
    protected:
        static auto max_size() -> size_type
        {
            return std::numeric_limits<size_type>::max();
        }
    private: // --- state ---
        size_type _capacity;
        size_type _size;
        std::unique_ptr<char[]> _data;
    protected: // --- life ---
        explicit repr() noexcept
            : _capacity(), _size()
        { }
        repr(const self& rhs)
            : _capacity(rhs._size), _size(rhs._size), _data(std::make_unique<char[]>(rhs._size))
        {
            traits_type::copy(_data.get(), rhs._data.get(), rhs._size);
        }
        repr(self&& rhs) noexcept
            : repr()
        {
            swap(rhs);
        }
        ~repr() noexcept = default;
        explicit repr(const extent<size_type>& extent)
            : _capacity(extent.capacity()), _size(extent.size()), _data(std::make_unique<char[]>(_capacity))
        { }
    protected: // --- operations ---
        auto operator=(const self& rhs) & -> self&
        {
            if (this == &rhs) {
                // nothing (required for following condition)
            } else if (_capacity >= rhs._size) {
                traits_type::copy(_data.get(), rhs._data.get(), rhs._size);
                _size = rhs._size;
            } else {
                self(rhs).swap(*this);
            }
            return *this;
        }
        auto operator=(self&& rhs) & noexcept -> self&
        {
            swap(rhs);
            return *this;
        }
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_capacity, rhs._capacity);
            up::swap_noexcept(_size, rhs._size);
            up::swap_noexcept(_data, rhs._data);
        }
        auto capacity() const noexcept -> size_type
        {
            return _capacity;
        }
        auto size() const noexcept -> size_type
        {
            return _size;
        }
        void set_size(size_type n) noexcept
        {
            _size = n;
        }
        auto data() const noexcept -> const char*
        {
            return _data.get();
        }
        auto data() noexcept -> char*
        {
            return _data.get();
        }
    };

    using shared_string = basic_string<repr, false>;
    using unique_string = basic_string<repr, true>;

    extern template class basic_string<repr, false>;
    extern template class basic_string<repr, true>;

}

namespace std
{

    template <typename Repr, bool Unique>
    class hash<up_string::basic_string<Repr, Unique>> final
    {
    public: // --- scope ---
        using argument_type = up_string::basic_string<Repr, Unique>;
        using result_type = std::size_t;
    public: // --- operations ---
        auto operator()(const up_string::basic_string<Repr, Unique>& value) const noexcept
        {
            using string_view = typename up_string::basic_string<Repr, Unique>::string_view;
            return hash<string_view>()(value.operator string_view());
        }
    };

}

namespace up
{

    using up_string::basic_string;
    using up_string::shared_string;
    using up_string::unique_string;

}
