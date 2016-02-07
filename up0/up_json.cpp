#include "up_json.hpp"


class up_json::json::value::impl
{
public: // --- state ---
    kind _kind;
public: // --- life ---
    explicit impl(kind kind)
        : _kind(kind)
    { }
    /* The destructor has to be declared virtual, so that dynamic_cast can be
     * used in combination with sub-classes (i.e. polymorphic type). */
    virtual ~impl() noexcept = default;
};


namespace
{

    class impl_boolean : public up_json::json::value::impl
    {
    public: // --- state ---
        bool _value;
    public: // --- life ---
        explicit impl_boolean(bool value)
            : impl(up_json::json::kind::boolean), _value(value)
        { }
    };

    class impl_number : public up_json::json::value::impl
    {
    public: // --- state ---
        double _value;
    public: // --- life ---
        explicit impl_number(double value)
            : impl(up_json::json::kind::number), _value(value)
        { }
    };

    class impl_string : public up_json::json::value::impl
    {
    public: // --- state ---
        up::istring _value;
    public: // --- life ---
        explicit impl_string(up::istring value)
            : impl(up_json::json::kind::string), _value(std::move(value))
        { }
    };

    class impl_array : public up_json::json::value::impl
    {
    public: // --- state ---
        up_json::json::array _values;
    public: // --- life ---
        explicit impl_array(up_json::json::array values)
            : impl(up_json::json::kind::array), _values(std::move(values))
        { }
    };

    class impl_object : public up_json::json::value::impl
    {
    public: // --- state ---
        up_json::json::object _values;
    public: // --- life ---
        explicit impl_object(up_json::json::object values)
            : impl(up_json::json::kind::object), _values(std::move(values))
        { }
    };

    template <typename Type>
    auto cast(const std::shared_ptr<const up_json::json::value::impl>& impl)
        -> decltype(auto)
    {
        if (impl) {
            return dynamic_cast<const Type&>(*impl);
        } else {
            up::throw_error<std::bad_cast>("up-json-cast");
        }
    }

}


up_json::json::value::value(std::nullptr_t unused __attribute__((unused)))
    : _impl()
{ }

up_json::json::value::value(bool value)
    : _impl(std::make_shared<const impl_boolean>(std::move(value)))
{ }

up_json::json::value::value(double value)
    : _impl(std::make_shared<const impl_number>(std::move(value)))
{ }

up_json::json::value::value(up::istring value)
    : _impl(std::make_shared<const impl_string>(std::move(value)))
{ }

up_json::json::value::value(up::string_view value)
    : _impl(std::make_shared<const impl_string>(up::istring(value)))
{ }

up_json::json::value::value(const char* value)
    : _impl(std::make_shared<const impl_string>(up::istring(value)))
{ }

up_json::json::value::value(array values)
    : _impl(std::make_shared<const impl_array>(std::move(values)))
{ }

up_json::json::value::value(object values)
    : _impl(std::make_shared<const impl_object>(std::move(values)))
{ }

auto up_json::json::value::get_kind() const -> kind
{
    return _impl ? _impl->_kind : kind::null;
}

auto up_json::json::value::get_boolean() const -> bool
{
    return cast<impl_boolean>(_impl)._value;
}

auto up_json::json::value::get_number() const -> double
{
    return cast<impl_number>(_impl)._value;
}

auto up_json::json::value::get_string() const -> const up::istring&
{
    return cast<impl_string>(_impl)._value;
}

auto up_json::json::value::get_array() const -> const array&
{
    return cast<impl_array>(_impl)._values;
}

auto up_json::json::value::get_object() const -> const object&
{
    return cast<impl_object>(_impl)._values;
}
