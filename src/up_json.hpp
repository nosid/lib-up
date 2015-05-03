#pragma once

/**
 * This file provides a data type to represent JSON objects, that is easy to
 * use and efficient.
 */

#include "up_linked_map.hpp"
#include "up_optional.hpp"
#include "up_terminate.hpp"

namespace up_json
{

    class json final
    {
    public: // --- scope ---
        enum class kind : uint8_t;
        class value;
    };


    enum class json::kind : uint8_t
    {
        null,
        boolean,
        number,
        string,
        array,
        object,
    };


    /**
     * Immutable class to represent JSON.
     */
    class json::value final
    {
    public: // --- scope ---
        using self = value;
        class impl;
    private: // --- state ---
        /* TODO: The implementation is not yet optimized for keeping the
         * memory usage low. */
        std::shared_ptr<const impl> _impl;
    public: // --- life ---
        // implicit
        value(std::nullptr_t);
        // implicit
        value(bool value);
        // implicit
        value(double value);
        // implicit
        value(up::istring value);
        // implicit
        value(const std::string& value);
        // implicit
        value(const char* value);
        // implicit
        value(std::vector<value> values);
        // implicit
        value(std::initializer_list<value> values);
        // implicit
        value(up::linked_map<up::istring, value> values);
        // avoid unintentional conversions
        template <typename... Types>
        value(Types&&...) = delete;
    public: // --- operations ---
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_impl, rhs._impl);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto get_kind() const -> kind;
        auto get_boolean() const -> bool;
        auto get_number() const -> double;
        auto get_string() const -> const up::istring&;
        auto get_array() const -> const std::vector<value>&;
        auto get_object() const -> const up::linked_map<up::istring, value>&;
        template <typename Visitor>
        auto accept(Visitor&& visitor) const
        {
            auto k = kind();
            switch (k) {
            case kind::null:
                return std::forward<Visitor>(visitor)(nullptr);
            case kind::boolean:
                return std::forward<Visitor>(visitor)(get_boolean());
            case kind::number:
                return std::forward<Visitor>(visitor)(get_number());
            case kind::string:
                return std::forward<Visitor>(visitor)(get_string());
            case kind::array:
                return std::forward<Visitor>(visitor)(get_array());
            case kind::object:
                return std::forward<Visitor>(visitor)(get_object());
            default:
                // Check this condition with kind()
                UP_TERMINATE("invalid json kind"_s, k);
            }
        }
    };

}

namespace up
{

    using up_json::json;

}
