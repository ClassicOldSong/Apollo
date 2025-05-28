// zero_width_pad.hpp
#pragma once
#include <string>
#include <bit>      // std::bit_width – C++20
#include <stdexcept>

namespace zwpad
{
    // Two distinct zero-width characters.
    // U+200B  ZERO WIDTH SPACE   – “0”
    // U+200C  ZERO WIDTH NON-JOINER – “1”
    inline constexpr char8_t ZW0[] = u8"\u200B";
    inline constexpr char8_t ZW1[] = u8"\u200C";

    /// \brief Encode \p index with a fixed-width binary prefix made of
    ///        zero-width code-points and append the original text.
    ///
    /// \param text     The payload you actually want to keep visible.
    /// \param padBits  How many zero-width *digits* to prepend.
    ///                 (Usually: std::bit_width(count-1).)
    /// \param index    Position in the ordered set, **0-based**.
    ///
    /// \return A UTF-8 std::string whose first \p padBits characters are
    ///         either U+200B or U+200C, followed by \p text.
    ///
    /// The lexical order of the resulting strings corresponds to the
    /// numerical order of *index* because U+200B < U+200C.
    ///
    inline std::string
    pad_for_ordering(std::string_view text,
                     std::size_t      padBits,
                     std::size_t      index)
    {
        if (padBits == 0)
            throw std::invalid_argument("padBits must be > 0");
        if (index >= (std::size_t{1} << padBits))
            throw std::out_of_range("index does not fit into padBits");

        std::string out;
        out.reserve(padBits * 3 + text.size());   // each ZW char is 3-byte UTF-8

        for (std::size_t bit = 0; bit < padBits; ++bit)
        {
            // Emit the *most* significant bit first.
            const bool one = (index >> (padBits - 1 - bit)) & 1;
            out += one ? reinterpret_cast<const char*>(ZW1)
                       : reinterpret_cast<const char*>(ZW0);
        }
        out.append(text);
        return out;
    }

    /// Convenience: compute the minimal pad width from the total count.
    [[nodiscard]]
    inline std::size_t pad_width_for_count(std::size_t count)
    {
        if (count == 0)
            throw std::invalid_argument("count must be > 0");
        return std::bit_width(count - 1); // e.g. count==8 → 3 bits
    }
} // namespace zwpad
