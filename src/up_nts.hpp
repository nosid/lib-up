#pragma once

#include "up_include.hpp"

#include <array>

#include "up_string_view.hpp"
#include "up_swap.hpp"

namespace up_nts
{

    class nts final
    {
    private: // --- scope ---
        using self = nts;
        using size_type = std::size_t;
        struct ref
        {
            char* _data;
            size_type _size;
        };
        static const constexpr size_type handle_size = std::max<size_type>(64, sizeof(ref) * 2);
        union handle
        {
            struct
            {
                ref _ref;
                std::array<char, handle_size - sizeof(ref)> _pad;
            } _padded;
            std::array<char, handle_size> _data;
        };
        static_assert(std::is_standard_layout<handle>::value, "requires standard layout");
        static_assert(sizeof(handle) == handle_size, "unexpected size mismatch");
    private: // --- state ---
        handle _handle;
    public: // --- life ---
        explicit nts() noexcept;
        explicit nts(const up::string_view& s);
        explicit nts(const char* data, size_type size);
        nts(const self& rhs) = delete;
        nts(self&& rhs) noexcept
            : nts()
        {
            swap(rhs);
        }
        ~nts() noexcept;
    public: // --- operations ---
        auto operator=(const self& rhs) & -> self& = delete;
        auto operator=(self&& rhs) & noexcept -> self&
        {
            swap(rhs);
            return *this;
        }
        void swap(self& rhs) noexcept
        {
            up::swap_noexcept(_handle._data, rhs._handle._data);
        }
        friend void swap(self& lhs, self& rhs) noexcept
        {
            lhs.swap(rhs);
        }
        operator const char*() const &&;
    };

}

namespace up
{

    using up_nts::nts;

}
