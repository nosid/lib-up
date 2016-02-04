#pragma once

#include "up_string_view.hpp"
#include "up_widen.hpp"

namespace up_source
{

    /**
     * TODO: Documentation and rationale, including:
     *
     * - information about the origin of an event with the source code,
     * - minimal overhead, in particular no memory allocation,
     * - implicitly constructible from a string literal,
     * - all member functions are declared as noexcept to make them easily
     *   usable in exception-handling code.
     */
    class source final
    {
    private: // --- scope ---
        using self = source;
        using line_t = int;
        /**
         * Internal helper class used to protect default parameters from being
         * explicitly specified by the caller. This class is private within
         * the enclosing class, so that it can only be used from within the
         * enclosing class, and not from outside.
         */
        class protect final
        {
        public: // --- life ---
            // note: constructor must be explicitly defined (i.e. not =default)
            explicit protect() noexcept { }
        };
    private: // --- fields ---
        /**
         * The label is a symbolic and if possible meaningful representation
         * of the origin of the event (in contrast to the file and line which
         * are physical representation without any further meaning). The label
         * must only consist of printable ASCII chars. It is not intended to
         * be used as an error message directly shown to a user (in order to
         * keep it as simple as possible).
         */
        const char* _label;
        std::size_t _size;
        const char* _file;
        line_t _line;
    public: // --- life ---
        /**
         * Implicit construction from string literal. Instances can directly
         * be constructed from string literals. On the other hand it is
         * intentionally hard to construct it from anything else to prevent
         * wrong usages.
         *
         * The constructor should only be called with a single argument,
         * because of the magic default arguments.
         */
        template <std::size_t N>
        source(
            const char (&label)[N],
            const protect& = protect{}, // protect the following default args
            const char* file = SOURCE_FILE(),
            line_t line = up::widen(SOURCE_LINE())) noexcept
            : _label(label), _size(N), _file(file), _line(line)
        {
            static_assert(N > 0, "invalid array for string literal");
        }
        /**
         * Prevent unintended conversions with a deleted catch-all
         * constructor.
         */
        template <typename... Args>
        source(Args&&...) noexcept = delete;
        /*
         * The 5 special member functions are explicitly declared because of
         * the above catch-all function. The copy constructor and copy
         * assignment operator are intentionally declared as noexcept to make
         * them easily usable in exception-handling code.
         */
        source(const self& rhs) noexcept = default;
        source(self&& rhs) noexcept = default;
        ~source() noexcept = default;
    public: // --- operations ---
        auto operator=(const self& rhs) & noexcept -> self& = default;
        auto operator=(self&& rhs) & noexcept -> self& = default;
        auto label() const noexcept -> up::string_view
        {
            return {_label, _size - 1};
        }
        auto label_c_str() const noexcept -> const char*
        {
            return _label;
        }
        auto file() const noexcept -> up::string_view
        {
            return {_file};
        }
        auto file_c_str() const noexcept -> const char*
        {
            return _file;
        }
        auto line() const noexcept -> line_t
        {
            return _line;
        }
    };

}

namespace up
{

    using up_source::source;

}
