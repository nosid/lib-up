#pragma once

#include "up_hash.hpp"

namespace up_istring
{

    /**
     * Immutable string with short string optimization (SSO). SSO is
     * particularly important for reprensenting XML and JSON fragements in
     * memory. Unfortunately, GCC-4.9 still has no good implementation.
     *
     * The secondary design goal was to keep the class as simple as possible.
     * For this reason, it is immutable and it provides only the required
     * member functions.
     *
     * This implementation uses a core size of 16 bytes on x86-64. That means,
     * strings with up to 15 characters can be stored inline, which should
     * work find for most strings in XML and JSON documents.
     */
    class istring final
    {
    public: // --- scope ---
        using self = istring;
        static constexpr unsigned char core_size = sizeof(char*) * 2;
    private: // --- state ---
        unsigned char _core[core_size];
    public: // --- life ---
        istring();
        istring(const char* data, std::size_t size);
        istring(const char* data);
        istring(up::chunk::from chunk);
        explicit istring(const std::string& string);
        istring(const self& rhs);
        istring(self&& rhs) noexcept;
        ~istring() noexcept;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self&;
        auto operator=(self&& rhs) & noexcept -> self&;
        void swap(self& rhs) noexcept;
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        auto size() const -> std::size_t;
        auto data() const -> const char*;
        operator up::chunk::from() const;
        auto to_string() const -> std::string;
        void out(std::ostream& os) const;
    };

}

namespace std
{

    template <>
    struct hash<up_istring::istring>
    {
        using argument_type = up_istring::istring;
        using result_type = std::size_t;
        auto operator()(const up_istring::istring& value) const noexcept
        {
            return up::fnv1a(value);
        }
    };
}

namespace up
{

    using up_istring::istring;

}
