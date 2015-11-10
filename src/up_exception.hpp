#pragma once

/**
 * This file contains an exception framework for this library. The primary
 * design goals of this framework are:
 *
 * (a) provide structured information about the cause of the exception, so
 *     that the information can be logged in a unique and safe manner (and
 *     optionally in different formats),
 *
 * (b) efficient implementation and lazy serialization (pay only if it is
 *     required),
 *
 * (c) make it easy to add exception types.
 */

#include "up_fabric.hpp"
#include "up_out.hpp"
#include "up_pack.hpp"
#include "up_source_location.hpp"

namespace up_exception
{

    // declaration only, will be partially specialized
    template <typename...>
    class exception;


    /**
     * The class template 'exception' realizes a hierarchy of exception
     * classes, with the specialization exception<> at the top. All exceptions
     * thrown from this library should be (directly or indirectly) derived
     * from this base class, so that they can be easily caught.
     *
     * In addition to std::exception, the class contains a member function to
     * return information about the cause of the exception in a structured
     * way. This information should only be used for logging and monitoring.
     */
    template <>
    class exception<> : public virtual std::exception
    {
    protected: // --- life ---
        virtual ~exception() noexcept = default;
    public: // --- operations ---
        auto to_fabric() const
        {
            return _to_fabric();
        }
    private:
        virtual auto _to_fabric() const -> up::fabric = 0;
    };


    /**
     * The template arguments are used to build a hierarchy. The template
     * types can be arbitrary. Even declaration-only types are fine.
     *
     * The class template supports arbitrary deep hierarchies. However,
     * currently one level is used, and it is still unclear, whether there is
     * any use in additional levels.
     */
    template <typename Head, typename... Tail>
    class exception<Head, Tail...> : public exception<Tail...> { };


    /**
     * Implementation of the exception classes with arbitrary tags and
     * arbitrary members (for providing structured information). The members
     * will be converted lazy into the structure required by the base class.
     */
    template <typename... Tags>
    class tagged final
    {
    public: // --- scope ---
        template <typename... Types>
        class throwable final : public exception<Tags...>
        {
        public: // --- scope ---
            using self = throwable;
        private: // --- state ---
            up::source_location _location;
            up::string_literal _message;
            up::pack<Types...> _args;
        public: // --- life ---
            template <typename... Args>
            explicit throwable(up::source_location location, up::string_literal message, Args&&... args)
                : _location(std::move(location))
                , _message(std::move(message))
                , _args(std::forward<Args>(args)...)
            { }
            throwable(const self& rhs) = delete;
            throwable(self&& rhs) noexcept
                : _location(std::move(rhs._location))
                , _message(std::move(rhs._message))
                , _args(std::move(rhs._args))
            { }
            ~throwable() noexcept = default;
        public: // --- operations ---
            auto operator=(const self& rhs) & -> self& = delete;
            auto operator=(self&& rhs) & noexcept -> self& = delete;
        private:
            virtual auto what() const noexcept -> const char* override
            {
                return _message.data();
            }
            virtual auto _to_fabric() const -> up::fabric override
            {
                return _to_fabric_aux(std::index_sequence_for<Types...>{});
            }
            template <std::size_t... Indexes>
            auto _to_fabric_aux(std::index_sequence<Indexes...>) const
            {
                return up::fabric(typeid(*this), _message.to_string(),
                    up::invoke_to_fabric_with_fallback(up::pack_get<Indexes>(_args))...);
            }
        };
    };


    /**
     * Helper function to simplify below macro and type deduction.
     */
    template <typename... Tags, typename... Args>
    [[noreturn]]
    void raise(up::source_location location, up::string_literal message, Args&&... args)
    {
        /* The arguments will always be copied into the thrown exception
         * object, because it is unknown where the exception will be thrown,
         * and the arguments might refer to local variables. */
        throw typename tagged<Tags...>::template throwable<std::decay_t<Args>...>{
            location, message, std::forward<Args>(args)...};
    }


    /**
     * The class can be used to add an error code to a raised exception. The
     * error code will be logged with a verbose error message (lazily).
     */
    class errno_info final
    {
    private: // --- state ---
        int _value;
    public: // --- life ---
        explicit errno_info(int value)
            : _value(value)
        { }
    public: // --- operations ---
        auto to_fabric() const -> up::fabric;
    };


    /**
     * Convenience function to log some information about the exception to the
     * given ostream.
     */
    void log_current_exception_aux(std::ostream& os);

    /**
     * Convenience function to log some information about the exception to the
     * given ostream, prefixed by the given arguments (for further
     * convenience).
     */
    template <typename... Args>
    void log_current_exception(std::ostream& os, Args&&... args)
    {
        up::out(os, std::forward<Args>(args)...);
        log_current_exception_aux(os);
    }


    /**
     * This function will be called whenever an exception is suppressed
     * because it is converted to an error code. That means the error is still
     * signaled. However, the information from the exception is lost.
     *
     * The function does basically nothing. The idea is to use this function
     * to mark all locations where exceptions are suppressed. In the future, a
     * handler for suppressed exceptions might be registered.
     */
    void suppress_current_exception(up::source_location location, up::string_literal message);

}

namespace up
{

    using up_exception::exception;
    using up_exception::errno_info;
    using up_exception::log_current_exception;

}

#define UP_RAISE(TAG, ...) \
    do { \
        using namespace up::literals; \
        ::up_exception::raise<TAG>(UP_SOURCE_LOCATION(), __VA_ARGS__); \
    } while (false)

#define UP_SUPPRESS_CURRENT_EXCEPTION(...) \
    do { \
        using namespace up::literals; \
        ::up_exception::suppress_current_exception(UP_SOURCE_LOCATION(), __VA_ARGS__); \
    } while (false)
