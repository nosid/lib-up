#pragma once

#include "up_optional.hpp"
#include "up_string.hpp"

namespace up_optional_string
{

    class optional_string final : private up::string_repr::handle<false, true>
    {
    public: // --- scope ---
        using self = optional_string;
        using value_type = up::shared_string;
    private:
        template <bool Nullable>
        using repr_type = up::string_repr::handle<false, Nullable>;
        using base = repr_type<true>;

    public: // --- life ---
        ~optional_string() noexcept = default;
        optional_string() noexcept
            : base(nullptr)
        { }
        optional_string(up::nullopt_t) noexcept
            : base(nullptr)
        { }
        optional_string(const self& rhs) = default;
        optional_string(self&& rhs) noexcept = default;
        optional_string(const value_type& rhs)
            : base(rhs.repr())
        { }
        optional_string(value_type&& rhs) noexcept
            : base(std::move(rhs).repr())
        { }
        template <typename... Args>
        explicit optional_string(up::in_place_t, Args&&... args)
            : base(value_type(std::forward<Args>(args)...).repr())
        { }

    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = default;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        auto operator=(up::nullopt_t) & noexcept -> self&
        {
            return operator=(self(up::nullopt));
        }
        auto operator=(const value_type& rhs) & -> self&
        {
            return operator=(self(rhs));
        }
        auto operator=(value_type&& rhs) & noexcept -> self&
        {
            return operator=(self(std::move(rhs)));
        }
        void swap(self& rhs) noexcept
        {
            base::swap(rhs);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }

        template <typename..., typename... Args>
        void emplace(Args&&... args)
        {
            self(std::forward<Args>(args)...).swap(*this);
        }

        // returns a proxy because it can't return a pointer
        auto operator->() const& -> up::optional<value_type>
        {
            if (_is_null()) {
                return up::nullopt;
            } else {
                return value_type(repr_type<false>(*this));
            }
        }
        auto operator->() && -> up::optional<value_type>
        {
            if (_is_null()) {
                return up::nullopt;
            } else {
                return value_type(repr_type<false>(std::move(*this)));
            }
        }
        auto operator*() const& -> value_type
        {
            return value_type(repr_type<false>(*this));
        }
        auto operator*() && -> value_type
        {
            return value_type(repr_type<false>(std::move(*this)));
        }
        using base::operator bool;
        auto value() const& -> value_type
        {
            if (_is_null()) {
                throw up::make_throwable<up::bad_optional_access>("optional-string");
            } else {
                return value_type(repr_type<false>(*this));
            }
        }
        auto value() && -> value_type
        {
            if (_is_null()) {
                throw up::make_throwable<up::bad_optional_access>("optional-string");
            } else {
                return value_type(repr_type<false>(std::move(*this)));
            }
        }
        template <typename..., typename Arg>
        auto value_or(Arg&& arg) const& -> value_type
        {
            if (_is_null()) {
                return static_cast<value_type>(std::forward<Arg>(arg));
            } else {
                return value_type(repr_type<false>(*this));
            }
        }
        template <typename..., typename Arg>
        auto value_or(Arg&& arg) && -> value_type
        {
            if (_is_null()) {
                return static_cast<value_type>(std::forward<Arg>(arg));
            } else {
                return value_type(repr_type<false>(std::move(*this)));
            }
        }

        auto repr() const& -> const repr_type<true>&
        {
            return *this;
        }
        auto repr() && -> repr_type<true>&&
        {
            return std::move(*this);
        }
        friend auto to_insight(const self& arg) -> up::insight
        {
            return arg
                ? up::insight(typeid(arg), "exists", up::invoke_to_insight_with_fallback(*arg))
                : up::insight(typeid(arg), "nullopt");
        }
        // avoid accidental (implicit) conversion from an actual type to optional
        friend void to_insight(const self&& arg) = delete;
    private:
        bool _is_null() const noexcept
        {
            return !operator bool();
        }
    };

    bool operator==(const optional_string& lhs, const optional_string& rhs) noexcept;
    bool operator!=(const optional_string& lhs, const optional_string& rhs) noexcept;
    bool operator<(const optional_string& lhs, const optional_string& rhs) noexcept;
    bool operator>(const optional_string& lhs, const optional_string& rhs) noexcept;
    bool operator<=(const optional_string& lhs, const optional_string& rhs) noexcept;
    bool operator>=(const optional_string& lhs, const optional_string& rhs) noexcept;

    bool operator==(const optional_string& lhs, up::nullopt_t) noexcept;
    bool operator==(up::nullopt_t, const optional_string& rhs) noexcept;
    bool operator!=(const optional_string& lhs, up::nullopt_t) noexcept;
    bool operator!=(up::nullopt_t, const optional_string& rhs) noexcept;
    bool operator<(const optional_string& lhs, up::nullopt_t) noexcept;
    bool operator<(up::nullopt_t, const optional_string& rhs) noexcept;
    bool operator>(const optional_string& lhs, up::nullopt_t) noexcept;
    bool operator>(up::nullopt_t, const optional_string& rhs) noexcept;
    bool operator<=(const optional_string& lhs, up::nullopt_t) noexcept;
    bool operator<=(up::nullopt_t, const optional_string& rhs) noexcept;
    bool operator>=(const optional_string& lhs, up::nullopt_t) noexcept;
    bool operator>=(up::nullopt_t, const optional_string& rhs) noexcept;

    bool operator==(const optional_string& lhs, const up::string_view& rhs);
    bool operator==(const up::string_view& lhs, const optional_string& rhs);
    bool operator!=(const optional_string& lhs, const up::string_view& rhs);
    bool operator!=(const up::string_view& lhs, const optional_string& rhs);
    bool operator<(const optional_string& lhs, const up::string_view& rhs);
    bool operator<(const up::string_view& lhs, const optional_string& rhs);
    bool operator>(const optional_string& lhs, const up::string_view& rhs);
    bool operator>(const up::string_view& lhs, const optional_string& rhs);
    bool operator<=(const optional_string& lhs, const up::string_view& rhs);
    bool operator<=(const up::string_view& lhs, const optional_string& rhs);
    bool operator>=(const optional_string& lhs, const up::string_view& rhs);
    bool operator>=(const up::string_view& lhs, const optional_string& rhs);

}

namespace std
{

    template <>
    class hash<up_optional_string::optional_string> final
    {
    public: // --- scope ---
        using argument_type = up_optional_string::optional_string;
        using result_type = std::size_t;
    public: // --- operations ---
        auto operator()(const up_optional_string::optional_string& value) const noexcept
            -> result_type;
    };

}

namespace up
{

    using up_optional_string::optional_string;

}
