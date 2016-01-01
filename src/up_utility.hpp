#pragma once

/**
 * This file contains some generic utility functions and classes, which are
 * commonly required, and which are too small to get a file on their own.
 */

#include "up_buffer.hpp"
#include "up_fabric.hpp"
#include "up_pack.hpp"
#include "up_source_location.hpp"
#include "up_swap.hpp"

namespace up_utility
{

    /**
     * Extract a readable form for the given type. This function should be
     * used for debugging purposes only, because the form might change in the
     * future.
     */
    auto type_display_name(const std::type_info& type_info) -> std::string;


    __attribute__((__format__(__printf__, 2, 0)))
    void cformat(up::buffer& buffer, const char* format, va_list ap);

    __attribute__((__format__(__printf__, 1, 0)))
    auto cformat(const char* format, va_list ap) -> std::string;


    template <typename Type>
    using underlying_type = std::underlying_type_t<Type>;

    template <typename Type>
    auto to_underlying_type(Type value)
    {
        return static_cast<underlying_type<Type>>(value);
    }

    template <typename Type>
    auto from_underlying_type(underlying_type<Type> value)
    {
        return static_cast<Type>(value);
    }


    // for internal use only
    [[noreturn]]
    void raise_enum_set_runtime_error(up::string_literal message, up::fabric fabric);


    /**
     * This class is intended to be used to build bitset types for a given
     * enumeration. For example, it can be used to pass options to a
     * functions, which can be or'ed in an arbitrary way.
     */
    template <typename Enum, typename Bits = unsigned int>
    class enum_set
    {
    private: // --- scope ---
        using underlying_type = typename std::underlying_type_t<Enum>;
        static_assert(std::is_unsigned<underlying_type>{}, "requires unsigned type");
        static_assert(std::is_unsigned<Bits>{}, "requires unsigned type");
    private: // --- state ---
        Bits _bits;
    public: // --- life ---
        enum_set() : _bits() { }
        enum_set(std::initializer_list<Enum> values)
            : enum_set()
        {
            for (auto&& value : values) {
                auto raw = static_cast<underlying_type>(value);
                if (raw < std::numeric_limits<underlying_type>::digits) {
                    _bits |= underlying_type(1) << raw;
                } else {
                    using namespace up::literals;
                    raise_enum_set_runtime_error("enum-value-out-of-range"_sl,
                        up::invoke_to_fabric_with_fallback(raw));
                }
            }
        }
    public: // --- operations ---
        auto to_string() const -> std::string
        {
            return up::invoke_to_string(_bits);
        }
        bool all() const { return true; }
        template <typename... Tail>
        bool all(Enum value, Tail... tail) const
        {
            return _is_set(value) && all(tail...);
        }
        bool any() const { return false; }
        template <typename... Tail>
        bool any(Enum value, Tail... tail) const
        {
            return _is_set(value) || any(tail...);
        }
        bool none() const { return true; }
        template <typename... Tail>
        bool none(Enum value, Tail... tail) const
        {
            return !_is_set(value) && none(tail...);
        }
    private:
        bool _is_set(Enum value) const
        {
            auto digits = std::numeric_limits<underlying_type>::digits;
            return static_cast<underlying_type>(value) < digits
                && (_bits & (underlying_type(1) << static_cast<underlying_type>(value)));
        }
    };


    /**
     * This class can be used to push thread-specific context information per
     * thread. The information is intended to be used for debugging purposes.
     *
     * An important design goal of the class is to keep the overhead as low as
     * possible. It avoids dynamic memory allocations by linking the objects
     * on the execution stacks to each other. It also makes use of some kind
     * of type erasure for the same purpose.
     */
    class context_frame_base
    {
    private: // --- scope ---
        static thread_local context_frame_base* top;
    public:
        using visitor = std::function<void(const up::source_location&, const up::fabrics&)>;
        static void walk(const visitor& visitor);
    private: // --- state ---
        context_frame_base* _parent;
    public: // --- life ---
        explicit context_frame_base()
            : _parent(top)
        {
            top = this;
        }
        context_frame_base(const context_frame_base& rhs) = delete;
        context_frame_base(context_frame_base&& rhs) noexcept = delete;
    protected:
        ~context_frame_base() noexcept
        {
            top = _parent;
        }
    public: // --- operations ---
        auto operator=(const context_frame_base& rhs) & -> context_frame_base& = delete;
        auto operator=(context_frame_base&& rhs) & noexcept -> context_frame_base& = delete;
    private:
        virtual void _walk(const visitor& visitor) const = 0;
    };


    /**
     * This function provides the only way to examine the information in the
     * context_frames.
     */
    void context_frame_walk(const context_frame_base::visitor& visitor);


    template <typename... Types>
    class context_frame final : context_frame_base
    {
    private: // --- state ---
        up::source_location _location;
        up::pack<Types...> _args;
    public: // --- life ---
        explicit context_frame(up::source_location location, Types&&... args)
            : _location(location), _args(std::forward<Types>(args)...)
        { }
    private: // --- operations ---
        virtual void _walk(const visitor& visitor) const override
        {
            return _walk_aux(visitor, std::index_sequence_for<Types...>{});
        }
        template <std::size_t... Indexes>
            void _walk_aux(const visitor& visitor, std::index_sequence<Indexes...>) const
        {
            visitor(_location,
                up::fabrics{
                    up::invoke_to_fabric_with_fallback(up::pack_get<Indexes>(_args))...});
        }
    };

    /**
     * Pay special attention to how this works with type inference: If an
     * rvalue is passed to this function, Arg is no reference and passed as is
     * to the class context_frame. In this case, the class context_frame will
     * move the value into the newly created object.
     */
    template <typename... Args>
    auto context_frame_deduce_type(Args&&... args) -> context_frame<Args...>;

}

namespace up
{

    using up_utility::type_display_name;
    using up_utility::cformat;
    using up_utility::to_underlying_type;
    using up_utility::from_underlying_type;
    using up_utility::enum_set;
    using up_utility::context_frame_walk;

}

#define UP_CONTEXT_FRAME_CONCATENATE_AUX(prefix, suffix) prefix ## suffix
#define UP_CONTEXT_FRAME_CONCATENATE(prefix, suffix) UP_CONTEXT_FRAME_CONCATENATE_AUX(prefix, suffix)

#define UP_CONTEXT_FRAME(...)                                      \
    __attribute__((unused)) \
    decltype(::up_utility::context_frame_deduce_type(__VA_ARGS__)) \
    UP_CONTEXT_FRAME_CONCATENATE(UP_CONTEXT_FRAME_STATE_, __COUNTER__){up::source_location(), __VA_ARGS__}
