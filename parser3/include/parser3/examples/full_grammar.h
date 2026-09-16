#pragma once

// Generated from grammars/full.grammar by tools/grammar_gen.py. Do not edit.

#include "parser3/core/grammar_element.h"
#include "parser3/core/grammar_stack.h"
#include "parser3/core/semantics.h"

namespace vera::parser3::full {

// An exact byte sequence terminal.
template <char... Chars>
using Lit = ExpandableRule<Char::Exact<Chars>...>;

// Consumes nothing when the next byte (if any) is not an identifier-continue byte.
struct NotNameChar : GrammarFrame<NotNameChar> {
    static constexpr bool Nullable = true;

    bool Feed(ContextView ctx) {
        if (ctx.chars.Empty()) {
            return ctx.chars.Finished() ? ctx.stack.Reduce(this, ctx) : true;
        }
        const unsigned c = static_cast<unsigned char>(ctx.chars.Front());
        const bool name =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '$';
        return name ? ctx.stack.Throw(ctx) : ctx.stack.Reduce(this, ctx);
    }
};

// A keyword: exact bytes bounded by a non-name byte.
template <char... Chars>
using Keyword = ExpandableRule<Lit<Chars...>, NotNameChar>;

// Forward declarations (struct rules may reference each other through ref()).
struct Type;
struct Signature;
struct Block;
struct Expression;
struct IfStatement;
struct UnaryExpression;

// Rules, in declaration order.
using LineComment =
    ExpandableRule<Lit<'/', '/'>, StarArray<Char::Complement<Char::Union<Char::Exact<'\r'>, Char::Exact<'\n'>>>>,
                   Optional<FirstMatch<Lit<'\r', '\n'>, Char::Exact<'\n'>, Char::Exact<'\r'>>>>;
using Whitespace =
    FirstMatch<Char::Union<Char::Exact<' '>, Char::Exact<'\t'>, Char::Exact<'\r'>, Char::Exact<'\n'>>, LineComment>;
using Ws = StarArray<Whitespace>;
using Gap = ExpandableRule<Whitespace, StarArray<Whitespace>>;
using IdentifierStart = Char::Union<Char::Range<'a', 'z'>, Char::Range<'A', 'Z'>, Char::Exact<'_'>>;
using IdentifierContinue = Char::Union<Char::Range<'a', 'z'>, Char::Range<'A', 'Z'>, Char::Exact<'_'>, Char::Exact<'$'>,
                                       Char::Range<'0', '9'>>;
using Identifier = ExpandableRule<IdentifierStart, StarArray<IdentifierContinue>, Ws>;
using EscapeSequence = Char::Union<Char::Exact<'\''>, Char::Exact<'"'>, Char::Exact<'n'>, Char::Exact<'r'>,
                                   Char::Exact<'t'>, Char::Exact<'0'>, Char::Exact<'\\'>>;
using DoubleQuoteCharacter =
    FirstMatch<Char::Complement<Char::Union<Char::Exact<'"'>, Char::Exact<'\\'>, Char::Exact<'\r'>, Char::Exact<'\n'>>>,
               ExpandableRule<Char::Exact<'\\'>, EscapeSequence>>;
using StringLiteral = ExpandableRule<Char::Exact<'"'>, StarArray<DoubleQuoteCharacter>, Char::Exact<'"'>, Ws>;
using Path = StringLiteral;
using DecimalLiteral = ExpandableRule<Char::Range<'0', '9'>, StarArray<Char::Range<'0', '9'>>>;
using FractionalPart =
    ExpandableRule<Char::Exact<'.'>, ExpandableRule<Char::Range<'0', '9'>, StarArray<Char::Range<'0', '9'>>>>;
using ExponentPart = ExpandableRule<Char::Union<Char::Exact<'e'>, Char::Exact<'E'>>,
                                    Optional<Char::Union<Char::Exact<'+'>, Char::Exact<'-'>>>,
                                    ExpandableRule<Char::Range<'0', '9'>, StarArray<Char::Range<'0', '9'>>>>;
using NumericSuffix = FirstMatch<ExpandableRule<FractionalPart, Optional<ExponentPart>>, ExponentPart>;
using NumericLiteral = ExpandableRule<DecimalLiteral, Optional<NumericSuffix>, Ws>;
using BooleanLiteral = ExpandableRule<FirstMatch<Keyword<'f', 'a', 'l', 's', 'e'>, Keyword<'t', 'r', 'u', 'e'>>, Ws>;
using Literal =
    FirstMatch<BooleanLiteral, NumericLiteral, StringLiteral, ExpandableRule<Keyword<'n', 'u', 'l', 'l'>, Ws>>;
using BasicType = ExpandableRule<FirstMatch<Keyword<'i', 'n', 't'>, Keyword<'n', 'u', 'm', 'b', 'e', 'r'>,
                                            Keyword<'b', 'o', 'o', 'l', 'e', 'a', 'n'>,
                                            Keyword<'s', 't', 'r', 'i', 'n', 'g'>, Keyword<'n', 'u', 'l', 'l'>>,
                                 Ws>;
using QualifiedName = ExpandableRule<Identifier, Optional<ExpandableRule<Char::Exact<'.'>, Ws, Identifier>>>;
using Annotation = ExpandableRule<Char::Exact<'@'>, Identifier>;
using Import = ExpandableRule<Keyword<'i', 'm', 'p', 'o', 'r', 't'>, Ws, Char::Exact<'*'>, Ws, Keyword<'a', 's'>, Gap,
                              Identifier, Keyword<'f', 'r', 'o', 'm'>, Gap, Path>;
using Field = ExpandableRule<Identifier, Char::Exact<':'>, Ws, RuleRef<Type>, Char::Exact<';'>, Ws>;
using FunctionDeclaration = ExpandableRule<Optional<ExpandableRule<Keyword<'a', 's', 'y', 'n', 'c'>, Gap>>,
                                           Keyword<'f', 'u', 'n', 'c', 't', 'i', 'o', 'n'>, Gap, Identifier,
                                           RuleRef<Signature>, RuleRef<Block>>;
using ClassDeclaration = ExpandableRule<StarArray<Annotation>, Keyword<'c', 'l', 'a', 's', 's'>, Gap, Identifier,
                                        Char::Exact<'{'>, Ws, StarArray<Field>, Char::Exact<'}'>, Ws>;
using Declaration = ExpandableRule<Optional<ExpandableRule<Keyword<'e', 'x', 'p', 'o', 'r', 't'>, Gap>>,
                                   FirstMatch<FunctionDeclaration, ClassDeclaration>>;
using Module = ExpandableRule<StarArray<Import>, StarArray<Declaration>>;
using Root = ExpandableRule<Ws, Module, Eof>;
using Parameter = ExpandableRule<Identifier, Char::Exact<':'>, Ws, RuleRef<Type>>;
using ParameterList = ExpandableRule<Parameter, StarArray<ExpandableRule<Char::Exact<','>, Ws, Parameter>>>;
using Initializer = ExpandableRule<Char::Exact<'='>, Ws, RuleRef<Expression>>;
using VariableDeclaration =
    ExpandableRule<Identifier, Char::Exact<':'>, Ws, RuleRef<Type>, Initializer, Char::Exact<';'>, Ws>;
using LocalDeclaration = ExpandableRule<Keyword<'l', 'e', 't'>, Gap, VariableDeclaration>;
using ParenthesizedType =
    ExpandableRule<Char::Exact<'('>, Ws, RuleRef<Type>,
                   Optional<ExpandableRule<Char::Exact<'|'>, Ws, Keyword<'n', 'u', 'l', 'l'>, Ws>>, Char::Exact<')'>,
                   Ws>;
using AsyncFunctionType = ExpandableRule<Keyword<'a', 's', 'y', 'n', 'c'>, Gap,
                                         Keyword<'f', 'u', 'n', 'c', 't', 'i', 'o', 'n'>, Ws, RuleRef<Signature>>;
using NonAsyncFunctionType = ExpandableRule<Keyword<'f', 'u', 'n', 'c', 't', 'i', 'o', 'n'>, Ws, RuleRef<Signature>>;
using FunctionType = FirstMatch<AsyncFunctionType, NonAsyncFunctionType>;
using WhileStatement = ExpandableRule<Keyword<'w', 'h', 'i', 'l', 'e'>, Gap, Char::Exact<'('>, Ws, RuleRef<Expression>,
                                      Char::Exact<')'>, Ws, RuleRef<Block>>;
using ForOfStatement = ExpandableRule<Keyword<'f', 'o', 'r'>, Gap, Char::Exact<'('>, Ws, Keyword<'l', 'e', 't'>, Gap,
                                      Identifier, Char::Exact<':'>, Ws, RuleRef<Type>, Keyword<'o', 'f'>, Gap,
                                      RuleRef<Expression>, Char::Exact<')'>, Ws, RuleRef<Block>>;
using BreakStatement = ExpandableRule<Keyword<'b', 'r', 'e', 'a', 'k'>, Ws, Char::Exact<';'>, Ws>;
using ContinueStatement = ExpandableRule<Keyword<'c', 'o', 'n', 't', 'i', 'n', 'u', 'e'>, Ws, Char::Exact<';'>, Ws>;
using ReturnStatement =
    ExpandableRule<Keyword<'r', 'e', 't', 'u', 'r', 'n'>, Gap, RuleRef<Expression>, Char::Exact<';'>, Ws>;
using TryStatement = ExpandableRule<Keyword<'t', 'r', 'y'>, Gap, RuleRef<Block>, Ws, Keyword<'c', 'a', 't', 'c', 'h'>,
                                    Gap, Char::Exact<'('>, Ws, Identifier, Ws, Char::Exact<')'>, Ws, RuleRef<Block>>;
using SimpleStatement =
    ExpandableRule<RuleRef<Expression>, Optional<ExpandableRule<Char::Exact<'='>, Ws, RuleRef<Expression>>>,
                   Char::Exact<';'>, Ws>;
using Statement = FirstMatch<RuleRef<IfStatement>, WhileStatement, ForOfStatement, BreakStatement, ContinueStatement,
                             ReturnStatement, TryStatement, SimpleStatement>;
using MultiplicativeOp = ExpandableRule<FirstMatch<Char::Exact<'*'>, Char::Exact<'/'>, Char::Exact<'%'>>, Ws>;
using MultiplicativeExpression =
    ExpandableRule<RuleRef<UnaryExpression>, StarArray<ExpandableRule<MultiplicativeOp, RuleRef<UnaryExpression>>>>;
using AdditiveOp = ExpandableRule<FirstMatch<Char::Exact<'+'>, Char::Exact<'-'>>, Ws>;
using AdditiveExpression =
    ExpandableRule<MultiplicativeExpression, StarArray<ExpandableRule<AdditiveOp, MultiplicativeExpression>>>;
using RelationalOp = ExpandableRule<FirstMatch<Lit<'<', '='>, Lit<'>', '='>, Char::Exact<'<'>, Char::Exact<'>'>>, Ws>;
using RelationalExpression =
    ExpandableRule<AdditiveExpression, Optional<ExpandableRule<RelationalOp, AdditiveExpression>>>;
using EqualityOp = ExpandableRule<FirstMatch<Lit<'=', '=', '='>, Lit<'!', '=', '='>>, Ws>;
using EqualityExpression =
    ExpandableRule<RelationalExpression, Optional<ExpandableRule<EqualityOp, RelationalExpression>>>;
using LogicalAndExpression =
    ExpandableRule<EqualityExpression, StarArray<ExpandableRule<Lit<'&', '&'>, Ws, EqualityExpression>>>;
using LogicalOrExpression =
    ExpandableRule<LogicalAndExpression, StarArray<ExpandableRule<Lit<'|', '|'>, Ws, LogicalAndExpression>>>;
using ExpressionList =
    ExpandableRule<RuleRef<Expression>, StarArray<ExpandableRule<Char::Exact<','>, Ws, RuleRef<Expression>>>>;
using ArrayLiteral = ExpandableRule<Char::Exact<'['>, Ws, Optional<ExpressionList>, Char::Exact<']'>, Ws>;
using PropertyValue = ExpandableRule<Identifier, Char::Exact<':'>, Ws, RuleRef<Expression>>;
using PropertyValueList = ExpandableRule<PropertyValue, StarArray<ExpandableRule<Char::Exact<','>, Ws, PropertyValue>>>;
using ObjectLiteral = ExpandableRule<Char::Exact<'{'>, Ws, Optional<PropertyValueList>, Char::Exact<'}'>, Ws>;
using LambdaExpression =
    ExpandableRule<Optional<ExpandableRule<Keyword<'a', 's', 'y', 'n', 'c'>, Gap>>,
                   Keyword<'f', 'u', 'n', 'c', 't', 'i', 'o', 'n'>, Ws, RuleRef<Signature>, RuleRef<Block>>;
using Operand = FirstMatch<Literal, ArrayLiteral, ObjectLiteral,
                           ExpandableRule<Char::Exact<'('>, Ws, RuleRef<Expression>, Char::Exact<')'>, Ws>,
                           LambdaExpression, Identifier>;
using Arguments = ExpandableRule<Char::Exact<'('>, Ws, Optional<ExpressionList>, Char::Exact<')'>, Ws>;
using Index = ExpandableRule<Char::Exact<'['>, Ws, RuleRef<Expression>, Char::Exact<']'>, Ws>;
using Selector = ExpandableRule<Char::Exact<'.'>, Ws, Identifier>;
using EnsureNotNull = ExpandableRule<Char::Exact<'!'>, Ws>;
using PrimaryExpression = ExpandableRule<Operand, StarArray<FirstMatch<Arguments, Index, Selector, EnsureNotNull>>>;
using UnaryOp = FirstMatch<ExpandableRule<Char::Exact<'-'>, Ws>, ExpandableRule<Char::Exact<'!'>, Ws>,
                           ExpandableRule<Keyword<'a', 'w', 'a', 'i', 't'>, Gap>>;
struct Type : ExpandableRule<FirstMatch<FunctionType, ParenthesizedType, QualifiedName, BasicType>,
                             StarArray<ExpandableRule<Char::Exact<'['>, Ws, Char::Exact<']'>, Ws>>> {};
struct Signature : ExpandableRule<Char::Exact<'('>, Ws, Optional<ParameterList>, Char::Exact<')'>, Ws, Char::Exact<':'>,
                                  Ws, RuleRef<Type>> {};
struct Block
    : ExpandableRule<Char::Exact<'{'>, Ws, StarArray<FirstMatch<LocalDeclaration, Statement>>, Char::Exact<'}'>, Ws> {};
struct Expression : LogicalOrExpression {};
struct IfStatement
    : ExpandableRule<
          Keyword<'i', 'f'>, Gap, Char::Exact<'('>, Ws, RuleRef<Expression>, Char::Exact<')'>, Ws, RuleRef<Block>,
          Optional<ExpandableRule<Keyword<'e', 'l', 's', 'e'>, Ws, FirstMatch<RuleRef<IfStatement>, RuleRef<Block>>>>> {
};
struct UnaryExpression : FirstMatch<ExpandableRule<UnaryOp, RuleRef<UnaryExpression>>, PrimaryExpression> {};

} // namespace vera::parser3::full
