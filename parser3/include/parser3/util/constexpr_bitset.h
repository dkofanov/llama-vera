#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace vera::parser3 {

template <std::size_t Size, std::uint64_t... Words>
struct ConstexprBitset;

namespace detail {

constexpr std::uint64_t RangeWordMask(std::size_t lo, std::size_t hi, std::size_t word) {
    const std::size_t base = word * 64;
    if (hi < base || lo > base + 63) {
        return 0;
    }
    const std::size_t start = lo > base ? lo - base : 0;
    const std::size_t end = hi < base + 63 ? hi - base : 63;
    return (~std::uint64_t{0} << start) & (~std::uint64_t{0} >> (63 - end));
}

template <std::size_t Size, std::size_t Index, class Sequence>
struct ExactBitImpl;

template <std::size_t Size, std::size_t Index, std::size_t... Words>
struct ExactBitImpl<Size, Index, std::index_sequence<Words...>> {
    using Type =
        ConstexprBitset<Size, ((Words == Index / 64) ? (std::uint64_t{1} << (Index % 64)) : std::uint64_t{0})...>;
};

template <std::size_t Size, std::size_t Lo, std::size_t Hi, class Sequence>
struct RangeBitsImpl;

template <std::size_t Size, std::size_t Lo, std::size_t Hi, std::size_t... Words>
struct RangeBitsImpl<Size, Lo, Hi, std::index_sequence<Words...>> {
    using Type = ConstexprBitset<Size, RangeWordMask(Lo, Hi, Words)...>;
};

} // namespace detail

// A value-less bitset: the bit pattern is encoded in the type, so two bitsets
// with the same pattern are the same type. Use the word-less `ConstexprBitset<Size>`
// form to build one (see `ExactBit` / `RangeBits`).
template <std::size_t Size, std::uint64_t... Words>
struct ConstexprBitset {
    static constexpr std::size_t WordBits = 64;
    static constexpr std::size_t WordCount = (Size + WordBits - 1) / WordBits;

    static_assert(sizeof...(Words) == 0 || sizeof...(Words) == WordCount, "ConstexprBitset word count mismatch");

    template <std::size_t Index>
    using ExactBit = typename detail::ExactBitImpl<Size, Index, std::make_index_sequence<WordCount>>::Type;

    template <std::size_t Lo, std::size_t Hi>
    using RangeBits = typename detail::RangeBitsImpl<Size, Lo, Hi, std::make_index_sequence<WordCount>>::Type;

    static constexpr std::array<std::uint64_t, WordCount> WordsArray{Words...};

    static constexpr std::uint64_t Word(std::size_t index) {
        static_assert(sizeof...(Words) == WordCount, "ConstexprBitset value form requires all words");
        return WordsArray[index];
    }

    static constexpr bool Test(std::size_t index) {
        return (Word(index / WordBits) >> (index % WordBits)) & 1;
    }

    static constexpr bool Any() {
        return ((Words != 0) || ...);
    }

    static constexpr bool None() {
        return !Any();
    }
};

} // namespace vera::parser3
