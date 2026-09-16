#pragma once

// Generated from grammars/minimal.grammar by tools/grammar_gen.py. Do not edit.

#include "parser3/core/grammar_element.h"
#include "parser3/core/grammar_stack.h"

namespace vera::parser3::minimal {

// An exact byte sequence terminal.
template <char... Chars>
using Lit = ExpandableRule<Char::Exact<Chars>...>;

// Forward declarations (struct rules may reference each other through ref()).

// Rules, in declaration order.
using Whitespace = Char::Union<Char::Exact<' '>, Char::Exact<'\t'>, Char::Exact<'\r'>, Char::Exact<'\n'>>;
using Ws = StarArray<Whitespace>;
using Gap = ExpandableRule<Whitespace, StarArray<Whitespace>>;
using IdentifierStart = Char::Union<Char::Range<'a', 'z'>, Char::Range<'A', 'Z'>, Char::Exact<'_'>>;
using IdentifierContinue = Char::Union<Char::Range<'a', 'z'>, Char::Range<'A', 'Z'>, Char::Exact<'_'>, Char::Exact<'$'>,
                                       Char::Range<'0', '9'>>;
using Identifier = ExpandableRule<IdentifierStart, StarArray<IdentifierContinue>>;
using BasicType = FirstMatch<Lit<'i', 'n', 't'>, Lit<'n', 'u', 'm', 'b', 'e', 'r'>,
                             Lit<'b', 'o', 'o', 'l', 'e', 'a', 'n'>, Lit<'s', 't', 'r', 'i', 'n', 'g'>>;
using Block = ExpandableRule<Char::Exact<'{'>, Ws, Char::Exact<'}'>, Ws>;
using Signature = ExpandableRule<Char::Exact<'('>, Ws, Char::Exact<')'>, Ws, Char::Exact<':'>, Ws, BasicType, Ws>;
using FunctionDeclaration =
    ExpandableRule<Lit<'f', 'u', 'n', 'c', 't', 'i', 'o', 'n'>, Gap, Identifier, Ws, Signature, Block>;
using ClassDeclaration =
    ExpandableRule<Lit<'c', 'l', 'a', 's', 's'>, Gap, Identifier, Ws, Char::Exact<'{'>, Ws, Char::Exact<'}'>, Ws>;
using Declaration = FirstMatch<FunctionDeclaration, ClassDeclaration>;
using Module = StarArray<Declaration>;
using Root = ExpandableRule<Ws, Module, Eof>;

} // namespace vera::parser3::minimal
