#pragma once

#include "up_chunk.hpp"
#include "up_string_view.hpp"

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
        static const constexpr std::size_t npos = std::size_t(-1);
        static const constexpr unsigned char core_size = sizeof(char*) * 2;
    private: // --- state ---
        unsigned char _core[core_size];
    public: // --- life ---
        istring();
        istring(const char* data, std::size_t size);
        istring(const char* data);
        explicit istring(up::string_view string); // explicit to avoid unintended copies
        explicit istring(up::chunk::from chunk);
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
        auto compare(const self& rhs) const noexcept -> int;
        auto compare(std::size_t lhs_pos, std::size_t lhs_n, const self& rhs) const -> int;
        auto compare(std::size_t lhs_pos, std::size_t lhs_n, const self& rhs, std::size_t rhs_pos, std::size_t rhs_n) const -> int;
        auto compare(const char* rhs) const -> int;
        auto compare(std::size_t lhs_pos, std::size_t lhs_n, const char* rhs) const -> int;
        auto compare(std::size_t lhs_pos, std::size_t lhs_n, const char* rhs, std::size_t rhs_n) const -> int;
        operator up::string_view() const;
        auto to_string() const -> std::string;
        void out(std::ostream& os) const;
    };

    inline bool operator==(const istring& lhs, const istring& rhs) noexcept
    {
        return lhs.compare(rhs) == 0;
    }

    inline bool operator!=(const istring& lhs, const istring& rhs) noexcept
    {
        return !(lhs == rhs);
    }

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
            return hash<up::string_view>()(value);
        }
    };
}

namespace up
{

    using up_istring::istring;

}
