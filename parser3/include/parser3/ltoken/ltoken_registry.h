#pragma once

#include <cstddef>
#include <cstdint>
#include <tuple>

namespace vera::parser3 {

#define VERA_LTOKEN_TYPES(X)                                                                                           \
    X(Invalid)                                                                                                         \
    X(End)                                                                                                             \
    X(Identifier)                                                                                                      \
    X(IntegerLiteral)                                                                                                  \
    X(FloatLiteral)                                                                                                    \
    X(StringLiteral)                                                                                                   \
    X(KwImport)                                                                                                        \
    X(KwAs)                                                                                                            \
    X(KwFrom)                                                                                                          \
    X(KwExport)                                                                                                        \
    X(KwAsync)                                                                                                         \
    X(KwFunction)                                                                                                      \
    X(KwClass)                                                                                                         \
    X(KwLet)                                                                                                           \
    X(KwIf)                                                                                                            \
    X(KwElse)                                                                                                          \
    X(KwWhile)                                                                                                         \
    X(KwFor)                                                                                                           \
    X(KwOf)                                                                                                            \
    X(KwBreak)                                                                                                         \
    X(KwContinue)                                                                                                      \
    X(KwReturn)                                                                                                        \
    X(KwTry)                                                                                                           \
    X(KwCatch)                                                                                                         \
    X(KwAwait)                                                                                                         \
    X(KwInt)                                                                                                           \
    X(KwNumber)                                                                                                        \
    X(KwBoolean)                                                                                                       \
    X(KwString)                                                                                                        \
    X(KwTrue)                                                                                                          \
    X(KwFalse)                                                                                                         \
    X(KwNull)                                                                                                          \
    X(OrOr)                                                                                                            \
    X(AndAnd)                                                                                                          \
    X(StrictEqual)                                                                                                     \
    X(StrictNotEqual)                                                                                                  \
    X(LessEqual)                                                                                                       \
    X(GreaterEqual)

#define VERA_LTOKEN_SINGLE_CHARS(X)                                                                                    \
    X(Space, ' ')                                                                                                      \
    X(Tab, '\t')                                                                                                       \
    X(CarriageReturn, '\r')                                                                                            \
    X(Newline, '\n')                                                                                                   \
    X(LParen, '(')                                                                                                     \
    X(RParen, ')')                                                                                                     \
    X(LBrace, '{')                                                                                                     \
    X(RBrace, '}')                                                                                                     \
    X(LBracket, '[')                                                                                                   \
    X(RBracket, ']')                                                                                                   \
    X(Colon, ':')                                                                                                      \
    X(Semicolon, ';')                                                                                                  \
    X(Comma, ',')                                                                                                      \
    X(Dot, '.')                                                                                                        \
    X(At, '@')                                                                                                         \
    X(Pipe, '|')                                                                                                       \
    X(Assign, '=')                                                                                                     \
    X(Less, '<')                                                                                                       \
    X(Greater, '>')                                                                                                    \
    X(Plus, '+')                                                                                                       \
    X(Minus, '-')                                                                                                      \
    X(Star, '*')                                                                                                       \
    X(Slash, '/')                                                                                                      \
    X(Percent, '%')                                                                                                    \
    X(Bang, '!')

#define VERA_LTOKEN_FRONTIERS(X) X(Frontier)

enum class LTokenKind : std::uint8_t {
#define VERA_LTOKEN_ENUM(Name) Name,
#define VERA_LTOKEN_CHAR_ENUM(Name, Character) Name,
    VERA_LTOKEN_TYPES(VERA_LTOKEN_ENUM) VERA_LTOKEN_SINGLE_CHARS(VERA_LTOKEN_CHAR_ENUM)
        VERA_LTOKEN_FRONTIERS(VERA_LTOKEN_ENUM)
#undef VERA_LTOKEN_CHAR_ENUM
#undef VERA_LTOKEN_ENUM
};

template <LTokenKind>
struct LTokenType;

#define VERA_LTOKEN_CLASS(Name)                                                                                        \
    struct Name {                                                                                                      \
        static constexpr LTokenKind Kind = LTokenKind::Name;                                                           \
        struct State;                                                                                                  \
    };                                                                                                                 \
    template <>                                                                                                        \
    struct LTokenType<LTokenKind::Name> {                                                                              \
        using Type = Name;                                                                                             \
    };
template <char Character_, LTokenKind Kind_>
struct SingleChar {
    static constexpr LTokenKind Kind = Kind_;
    static constexpr char Value = Character_;
    struct State {};
};

#define VERA_LTOKEN_CHAR_CLASS(Name, Character)                                                                        \
    using Name = SingleChar<Character, LTokenKind::Name>;                                                              \
    template <>                                                                                                        \
    struct LTokenType<LTokenKind::Name> {                                                                              \
        using Type = Name;                                                                                             \
    };
VERA_LTOKEN_TYPES(VERA_LTOKEN_CLASS)
VERA_LTOKEN_SINGLE_CHARS(VERA_LTOKEN_CHAR_CLASS)
VERA_LTOKEN_FRONTIERS(VERA_LTOKEN_CLASS)
#undef VERA_LTOKEN_CHAR_CLASS
#undef VERA_LTOKEN_CLASS

#define VERA_LTOKEN_STATE(Name)                                                                                        \
    struct Name::State {};
VERA_LTOKEN_TYPES(VERA_LTOKEN_STATE)
#undef VERA_LTOKEN_STATE

struct Frontier::State {};

template <class Token>
inline constexpr LTokenKind KindOf = Token::Kind;

template <LTokenKind Kind>
using TypeOf = typename LTokenType<Kind>::Type;

template <LTokenKind Kind>
using StateOf = typename TypeOf<Kind>::State;

struct LTokenStateSentinel {};

using LTokenStates = std::tuple<
#define VERA_LTOKEN_STATE_TYPE(Name) Name::State,
#define VERA_LTOKEN_CHAR_STATE_TYPE(Name, Character) Name::State,
    VERA_LTOKEN_TYPES(VERA_LTOKEN_STATE_TYPE) VERA_LTOKEN_SINGLE_CHARS(VERA_LTOKEN_CHAR_STATE_TYPE)
        VERA_LTOKEN_FRONTIERS(VERA_LTOKEN_STATE_TYPE)
#undef VERA_LTOKEN_CHAR_STATE_TYPE
#undef VERA_LTOKEN_STATE_TYPE
            LTokenStateSentinel>;

#undef VERA_LTOKEN_FRONTIERS
#undef VERA_LTOKEN_SINGLE_CHARS
#undef VERA_LTOKEN_TYPES

} // namespace vera::parser3
