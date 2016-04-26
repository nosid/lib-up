#pragma once

/**
 * This class provides an implementation for so-called variable-length
 * quantities. See the following URL for more information:
 *
 * http://en.wikipedia.org/wiki/Variable-length_quantity
 *
 * The class is especially useful for:
 *
 * - compact representations of unsigned integers,
 * - and for storing unsigned integers in a machine independent format.
 */

#include "up_chunk.hpp"


namespace up_vlq
{

    // for internal use only
    [[noreturn]]
    void raise_vlq_overflow_error(std::size_t offset, uintmax_t value, uintmax_t limit);

    // for internal use only
    [[noreturn]]
    void raise_vlq_incomplete_error(std::size_t offset, uintmax_t value);


    template <typename Integral, typename Type, int bits = std::numeric_limits<Type>::digits - 1>
    class basic_vlq final
    {
    private: // --- scope ---
        static_assert(std::is_unsigned<Integral>{});
        static_assert(std::is_unsigned<Type>{});
        static_assert(bits < std::numeric_limits<Type>::digits);
        using array = std::array<Type, (std::numeric_limits<Integral>::digits - 1) / bits + 1>;
        using tuple = std::tuple<std::size_t, array>;
        /* This function encodes the given value into exactly N bytes. N
         * should be sufficiently large. If it is too large, the unnecessary
         * leading bytes will be zero. However, it is an error to invoke the
         * function with a too small value.
         *
         * The size N is a template parameter, so that the function will be
         * initiated once for each size (and Integral type). The idea is to
         * improve compiler optimization, although that has never been
         * verified. */
        template <std::size_t N>
        static auto encode_aux(Integral value, std::true_type) -> tuple
        {
            constexpr Integral _1 = 1;
            constexpr Integral msb = _1 << bits; // most significant bit
            constexpr Integral mask = msb - _1; // mask for other bits
            array result;
            std::size_t i = N;
            result[--i] = value & mask;
            while (i > 0) {
                value >>= bits;
                result[--i] = (value & mask) | msb;
            }
            return tuple(N, result);
        }
        template <std::size_t N>
        static auto encode_aux(Integral value, std::false_type) -> tuple
        {
            constexpr Integral _1 = 1;
            if (value < (_1 << (N * bits))) {
                return encode_aux<N>(value, std::true_type());
            } else {
                return encode<N + 1>(value);
            }
        }
    public:
        template <std::size_t N>
        static auto encode(Integral value) -> tuple
        {
            /* VLQ stores the most significant bits in the first byte.
             * Unfortunately, this makes the implementation a bit more
             * complicated. We calculate the required length, and use it to
             * encode the value in one (further) pass. */
            constexpr bool finish = N * bits >= std::numeric_limits<Integral>::digits;
            return encode_aux<N>(value, std::integral_constant<bool, finish>{});
        }
        static auto decode(const Type* array, std::size_t size)
            -> std::tuple<Integral, std::size_t>
        {
            constexpr std::size_t digits = std::numeric_limits<Integral>::digits;
            constexpr std::size_t safe = digits / bits;
            constexpr Integral _1 = 1;
            constexpr Integral msb = _1 << bits; // most significant bit
            constexpr Integral mask = msb - _1; // mask for other bits
            constexpr Integral limit = _1 << (digits - bits); // for overflow checking
            Integral result = 0;
            for (std::size_t i = 0; i != size; ++i) {
                if (i >= safe && result >= limit) {
                    raise_vlq_overflow_error(i, result, limit);
                }
                result = (result << bits) | (array[i] & mask);
                if (array[i] & msb) {
                    return std::make_tuple(result, i + 1);
                }
            }
            raise_vlq_incomplete_error(size, result);
        }
    };


    class vlq final
    {
    private: // --- scope ---
        static_assert(std::numeric_limits<unsigned char>::digits == 8);
        static_assert(std::numeric_limits<unsigned char>::max() == 255);
    public:
        template <typename Integral>
        static auto encode(Integral value)
        {
            return basic_vlq<Integral, unsigned char>::template encode<1>(value);
        }
        template <typename Integral>
        static auto decode(up::chunk::from chunk)
        {
            return basic_vlq<Integral, unsigned char>::decode(chunk.data(), chunk.size());
        }
    };

}

namespace up
{

    using up_vlq::vlq;

}
