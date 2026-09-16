#pragma once

// Generated from grammars/signal_demo.grammar by tools/grammar_gen.py. Do not edit.

#include "parser3/core/grammar_element.h"
#include "parser3/core/grammar_stack.h"
#include "parser3/core/semantics.h"

namespace vera::parser3::signal_demo {

// Forward declarations (struct rules may reference each other through ref()).

// Semantic tags. Specialize SignalAction<nota::Tag> for each element.
namespace nota {
struct Begin;
struct Char;
struct End;
} // namespace nota

// Rules, in declaration order.
using Ws = StarArray<Char::Union<Char::Exact<' '>, Char::Exact<'\t'>, Char::Exact<'\r'>, Char::Exact<'\n'>>>;
using Start = Char::Union<Char::Range<'a', 'z'>, Char::Range<'A', 'Z'>, Char::Exact<'_'>>;
using Cont = Char::Union<Char::Range<'a', 'z'>, Char::Range<'A', 'Z'>, Char::Range<'0', '9'>, Char::Exact<'_'>>;
using Name = ExpandableRule<Signal<nota::Begin>, Start, Signal<nota::Char>,
                            StarArray<ExpandableRule<Cont, Signal<nota::Char>>>, Signal<nota::End>>;
using Root = ExpandableRule<Name, Ws, Eof>;

} // namespace vera::parser3::signal_demo
