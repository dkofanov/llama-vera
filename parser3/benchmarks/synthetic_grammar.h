#pragma once

// Synthetic stress language shared by the parser3 benchmarks.
//
//   unit ::= stmt '\n'
//   stmt ::= "let"   ' ' ident ';'      // three lowercase letters
//           | "level" ' ' num   ';'      // three digits
//
// Every unit is a "level ..." statement, so a choice always tries "let" first
// and backtracks at the third byte ('v' != 't'), exercising Throw/Catch.

#include "parser3/core/grammar_element.h"
#include "parser3/core/grammar_stack.h"

#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace parser3_bench {

using namespace vera::parser3;

using KwLet = ExpandableRule<Char::Exact<'l'>, Char::Exact<'e'>, Char::Exact<'t'>>;
using KwLevel = ExpandableRule<Char::Exact<'l'>, Char::Exact<'e'>, Char::Exact<'v'>, Char::Exact<'e'>, Char::Exact<'l'>>;
using Ident = ExpandableRule<Char::Range<'a', 'z'>, Char::Range<'a', 'z'>, Char::Range<'a', 'z'>>;
using Number = ExpandableRule<Char::Range<'0', '9'>, Char::Range<'0', '9'>, Char::Range<'0', '9'>>;
using LetStmt = ExpandableRule<KwLet, Char::Exact<' '>, Ident, Char::Exact<';'>>;
using LevelStmt = ExpandableRule<KwLevel, Char::Exact<' '>, Number, Char::Exact<';'>>;
using Stmt = FirstMatch<LetStmt, LevelStmt>;
using Unit = ExpandableRule<Stmt, Char::Exact<'\n'>>;

// N-fold sequence of `Unit` as a single root, built entirely at compile time.
template <class Element, std::size_t Count, class Sequence>
struct RepeatTupleImpl;
template <class Element, std::size_t Count, std::size_t... Indices>
struct RepeatTupleImpl<Element, Count, std::index_sequence<Indices...>> {
    template <std::size_t>
    using Item = Element;
    using Type = std::tuple<Item<Indices>...>;
};
template <class Element, std::size_t Count>
using RepeatTuple = typename RepeatTupleImpl<Element, Count, std::make_index_sequence<Count>>::Type;

template <class Tuple>
struct ExpandableRuleOf;
template <class... Elements>
struct ExpandableRuleOf<std::tuple<Elements...>> {
    using Type = ExpandableRule<Elements...>;
};

// Number of units in the compile-time root sequence. Lower it for profiling
// builds, where aggressively optimized instantiation of the full sequence is
// memory-hungry.
#ifndef PARSER3_BENCH_UNITS
#define PARSER3_BENCH_UNITS 256
#endif

inline constexpr std::size_t kUnits = PARSER3_BENCH_UNITS;
inline constexpr std::size_t kStackSlots = 4096;
inline constexpr const char *kUnitText = "level 123;\n";

using Root = typename ExpandableRuleOf<RepeatTuple<Unit, kUnits>>::Type;

inline std::string MakeDocument() {
    std::string document;
    document.reserve(kUnits * 11);
    for (std::size_t i = 0; i < kUnits; ++i) {
        document += kUnitText;
    }
    return document;
}

// Equivalent GBNF for engines that accept arbitrary grammars (llama.cpp).
inline std::string MakeGbnf() {
    return "root ::= unit*\n"
           "unit ::= (\"let \" [a-z] [a-z] [a-z] \";\" | \"level \" [0-9] [0-9] [0-9] \";\") \"\\n\"\n";
}

} // namespace parser3_bench
