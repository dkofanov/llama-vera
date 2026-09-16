#!/usr/bin/env python3
"""Translate a small grammar description into a parser3 grammar header.

The translation is deliberately local and predictable: every construct maps to
one parser3 element, rules are emitted in file order, and recursion is spelled
out (`struct` + `ref`).  There is no whole-program analysis.

Description syntax
------------------
An item is either an include or a rule::

    %include "vera_semantics.h"
    alias  ws = ns*
    struct type = (ref(function-type) | basic-type) (exact('[') ws exact(']') ws)* @sem(Type)

`%include` reads a file relative to the grammar and inlines its text (minus a
leading ``#pragma once``) at global scope, so the emitted header is
self-contained and the bindings have no other consumer.

`alias` becomes ``using Name = <expr>;``; `struct` is forward declared and
becomes ``struct Name : <expr> {};``.  A rule is emitted where it is written, so
an alias must appear after the aliases it names; a `struct` is forward declared
and must be referenced with ``ref(Name)``.

Expressions
-----------
::

    alt      := seq ('|' seq)*
    seq      := postfix*                 # juxtaposition
    postfix  := atom ('*' | '+' | '?')*
    atom     := '(' expr ')'
              | STRING                   # lit("...")
              | CHAR                     # exact('x')
              | CLASS                    # [a-z0-9] / [^...]
              | 'EOF' | 'eof()'
              | NAME                     # reference to an earlier alias
              | NAME '(' [expr (',' expr)*] ')'

Constructors
------------
    seq, first, star, plus, opt     combinators
    ref(Name)                       RuleRef<Name>
    exact('x'), range('a','z')      Char::Exact / Char::Range
    union(...), complement(...)     Char::Union / Char::Complement
    lit("abc"), kw("abc")           byte terminal / keyword with name boundary
    eof()                           Eof

Sugar (documented desugaring, no inference)::

    A B C     -> seq(A, B, C)         ExpandableRule<A, B, C>
    A | B     -> first(A, B)          FirstMatch<A, B>
    A*  A+  A?-> star/plus/opt        StarArray / ExpandableRule<A, StarArray<A>> / Optional
    "abc"     -> lit("abc")
    'x'       -> exact('x')
    [a-z0-9]  -> union(range('a','z'), range('0','9'))
    [^...]    -> complement(...)

Semantic elements
-----------------
Semantic elements are the only ``@``-prefixed items; classic grammar elements
carry no ``@``.  Each is an invocation point of a semantic-engine binding:

    @name(args)            zero-width marker -> Signal<nota::Tag>
    @name(args){ expr }    region -> @name(begin,args) expr @name(end,args)

A region is sugar for a matched begin/end marker pair around its body (``begin``
/``end`` are prepended to the arguments).  ``Tag`` is ``camel(name)`` joined with
``camel(arg)`` by ``_`` (e.g. ``@name{ x }`` -> ``nota::Name_Begin`` /
``nota::Name_End``; ``@scope(Class){ x }`` -> ``nota::Scope_Begin_Class`` /
``nota::Scope_End_Class``).  Tags are forward-declared in a nested
``namespace nota``; the author header specializes ``SignalAction<nota::Tag>``.
A binding may update semantic state and returns false to reject the feed — a
hard failure, not a backtracking throw.  Arguments must be attached with no
space (``@x(y)``) so ``@x (y)*`` means ``@x`` applied to the group ``(y)*``.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


# ---------------------------------------------------------------------------
# AST
# ---------------------------------------------------------------------------


@dataclass
class Seq:
    elements: List[object] = field(default_factory=list)


@dataclass
class Alt:
    alternatives: List[object] = field(default_factory=list)


@dataclass
class Star:
    element: object


@dataclass
class Plus:
    element: object


@dataclass
class Opt:
    element: object


@dataclass
class Lit:
    text: str


@dataclass
class Kw:
    text: str


@dataclass
class Exact:
    char: str


@dataclass
class Range:
    lo: str
    hi: str


@dataclass
class Union:
    parts: List[object] = field(default_factory=list)


@dataclass
class Complement:
    element: object


@dataclass
class Class:
    segments: List[Tuple[int, int]] = field(default_factory=list)
    negated: bool = False


@dataclass
class Name:
    name: str


@dataclass
class Ref:
    name: str


@dataclass
class Eof:
    pass


@dataclass
class Directive:
    name: str
    args: List[str] = field(default_factory=list)


@dataclass
class Region:
    name: str
    args: List[str]
    body: object


@dataclass
class Rule:
    kind: str  # "alias" or "struct"
    name: str
    expr: object
    index: int = 0


@dataclass
class Include:
    path: str


class ParseError(Exception):
    pass


# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------

_NAME_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_-]*")
_ESCAPES = {"n": "\n", "r": "\r", "t": "\t", "0": "\0", "\\": "\\", '"': '"', "'": "'"}
_CONSTRUCTORS = {
    "seq",
    "first",
    "star",
    "plus",
    "opt",
    "ref",
    "exact",
    "range",
    "union",
    "complement",
    "lit",
    "kw",
    "eof",
}
_KEYWORDS = {"alias", "struct"}


class Parser:
    def __init__(self, text: str, source: str):
        self.s = text
        self.source = source
        self.i = 0
        self.n = len(text)

    def error(self, message: str) -> None:
        line = self.s.count("\n", 0, self.i) + 1
        col = self.i - (self.s.rfind("\n", 0, self.i) + 1) + 1
        raise ParseError(f"{self.source}:{line}:{col}: {message}")

    # -- lexing -------------------------------------------------------------
    def skip_ws(self) -> None:
        while self.i < self.n:
            c = self.s[self.i]
            if c in " \t\r\n":
                self.i += 1
            elif c == "#":
                while self.i < self.n and self.s[self.i] != "\n":
                    self.i += 1
            else:
                return

    def parse_name(self) -> str:
        match = _NAME_RE.match(self.s, self.i)
        if not match:
            self.error("expected a name")
        self.i = match.end()
        return match.group(0)

    def peek_name(self) -> Optional[str]:
        saved = self.i
        self.skip_ws()
        match = _NAME_RE.match(self.s, self.i)
        name = match.group(0) if match else None
        self.i = saved
        return name

    def next_non_ws(self) -> str:
        j = self.i
        while j < self.n and self.s[j] in " \t\r\n":
            j += 1
        return self.s[j] if j < self.n else ""

    def decode_escape(self) -> str:
        if self.i >= self.n:
            self.error("unterminated escape")
        e = self.s[self.i]
        self.i += 1
        if e not in _ESCAPES:
            self.error(f"unknown escape '\\{e}'")
        return _ESCAPES[e]

    def parse_string(self) -> str:
        assert self.s[self.i] == '"'
        self.i += 1
        out: List[str] = []
        while True:
            if self.i >= self.n:
                self.error("unterminated string literal")
            c = self.s[self.i]
            if c == '"':
                self.i += 1
                return "".join(out)
            if c == "\\":
                self.i += 1
                out.append(self.decode_escape())
            else:
                out.append(c)
                self.i += 1

    def parse_char(self) -> str:
        assert self.s[self.i] == "'"
        self.i += 1
        if self.i >= self.n:
            self.error("unterminated character literal")
        c = self.s[self.i]
        if c == "\\":
            self.i += 1
            value = self.decode_escape()
        else:
            value = c
            self.i += 1
        if self.i >= self.n or self.s[self.i] != "'":
            self.error("expected closing \"'\"")
        self.i += 1
        return value

    def parse_class(self) -> Class:
        assert self.s[self.i] == "["
        self.i += 1
        negated = False
        if self.i < self.n and self.s[self.i] == "^":
            negated = True
            self.i += 1
        chars: List[str] = []
        while True:
            if self.i >= self.n:
                self.error("unterminated character class")
            c = self.s[self.i]
            if c == "]":
                self.i += 1
                break
            if c == "\\":
                self.i += 1
                chars.append(self.decode_escape())
            else:
                chars.append(c)
                self.i += 1
        if not chars:
            self.error("empty character class")
        return Class(_class_segments(chars, self.error), negated)

    # -- grammar ------------------------------------------------------------
    def parse(self) -> List[object]:
        items: List[object] = []
        while True:
            self.skip_ws()
            if self.i >= self.n:
                return items
            if self.s[self.i] == "%":
                self.i += 1
                directive = self.parse_name()
                if directive != "include":
                    self.error(f"unknown directive '%{directive}'")
                self.skip_ws()
                if self.i >= self.n or self.s[self.i] != '"':
                    self.error("expected a quoted path after %include")
                items.append(Include(self.parse_string()))
                continue
            kind = self.parse_name()
            if kind not in _KEYWORDS:
                self.error(f"expected 'alias' or 'struct', got '{kind}'")
            self.skip_ws()
            name = self.parse_name()
            self.skip_ws()
            if self.i >= self.n or self.s[self.i] != "=":
                self.error(f"expected '=' after rule '{name}'")
            self.i += 1
            expr = self.parse_expr()
            items.append(Rule(kind, name, expr, len(items)))

    def parse_expr(self) -> object:
        alternatives = [self.parse_sequence()]
        while True:
            self.skip_ws()
            if self.i < self.n and self.s[self.i] == "|":
                self.i += 1
                alternatives.append(self.parse_sequence())
            else:
                break
        return alternatives[0] if len(alternatives) == 1 else Alt(alternatives)

    def parse_sequence(self) -> Seq:
        elements: List[object] = []
        while True:
            self.skip_ws()
            if self.i >= self.n or self.s[self.i] in ")}|":
                break
            if self.s[self.i] == "%":
                break
            if self.s[self.i] == "@":
                elements.append(self.parse_semantic())
                continue
            if self.peek_name() in _KEYWORDS:
                break
            elements.append(self.parse_postfix())
        return Seq(elements)

    # A semantic element: either a zero-width marker `@name(args)` or a
    # body-spanning region `@name(args){ expr }` (matched braces). Arguments must
    # be attached with no space, so '@x (y)*' is '@x' applied to the group
    # '(y)*' rather than '@x' called with argument 'y'.
    def parse_semantic(self) -> object:
        assert self.s[self.i] == "@"
        self.i += 1
        name = self.parse_name()
        args: List[str] = []
        if self.i < self.n and self.s[self.i] == "(":
            self.i += 1
            while True:
                self.skip_ws()
                if self.i < self.n and self.s[self.i] == ")":
                    self.i += 1
                    break
                if self.i < self.n and self.s[self.i] in "\"'":
                    args.append(self.parse_string() if self.s[self.i] == '"' else self.parse_char())
                else:
                    args.append(self.parse_name())
                self.skip_ws()
                if self.i < self.n and self.s[self.i] == ",":
                    self.i += 1
        self.skip_ws()
        if self.i < self.n and self.s[self.i] == "{":
            self.i += 1
            body = self.parse_expr()
            self.skip_ws()
            if self.i >= self.n or self.s[self.i] != "}":
                self.error("expected '}' to close a semantic region")
            self.i += 1
            return Region(name, args, body)
        return Directive(name, args)

    def parse_postfix(self) -> object:
        node = self.parse_atom()
        while True:
            self.skip_ws()
            if self.i < self.n and self.s[self.i] in "*+?":
                op = self.s[self.i]
                self.i += 1
                node = {"*": Star, "+": Plus, "?": Opt}[op](node)
            else:
                break
        return node

    def parse_atom(self) -> object:
        self.skip_ws()
        if self.i >= self.n:
            self.error("unexpected end of input")
        c = self.s[self.i]
        if c == "(":
            self.i += 1
            node = self.parse_expr()
            self.skip_ws()
            if self.i >= self.n or self.s[self.i] != ")":
                self.error("expected ')'")
            self.i += 1
            return node
        if c == '"':
            return Lit(self.parse_string())
        if c == "'":
            return Exact(self.parse_char())
        if c == "[":
            return self.parse_class()
        name = self.parse_name()
        if name in _CONSTRUCTORS:
            if self.next_non_ws() != "(":
                self.error(f"constructor '{name}' requires arguments")
            return self.parse_call(name)
        if name == "EOF" and self.next_non_ws() != "(":
            return Eof()
        return Name(name)

    def parse_call(self, name: str) -> object:
        if name not in _CONSTRUCTORS:
            self.error(f"unknown constructor '{name}'")
        self.skip_ws()
        assert self.s[self.i] == "("
        self.i += 1

        if name == "ref":
            self.skip_ws()
            target = self.parse_name()
            self.expect_close()
            return Ref(target)
        if name == "exact":
            self.skip_ws()
            char = self.parse_char()
            self.expect_close()
            return Exact(char)
        if name == "range":
            self.skip_ws()
            lo = self.parse_char()
            self.skip_ws()
            self.expect_comma()
            hi = self.parse_char()
            self.expect_close()
            return Range(lo, hi)
        if name in ("lit", "kw"):
            self.skip_ws()
            text = self.parse_string()
            self.expect_close()
            return Lit(text) if name == "lit" else Kw(text)
        if name == "eof":
            self.expect_close()
            return Eof()

        args = self.parse_arg_list()
        if name == "seq":
            return Seq(args)
        if name == "first":
            return Alt(args)
        if name == "star":
            self.require_arity(name, args, 1)
            return Star(args[0])
        if name == "plus":
            self.require_arity(name, args, 1)
            return Plus(args[0])
        if name == "opt":
            self.require_arity(name, args, 1)
            return Opt(args[0])
        if name == "union":
            return Union(args)
        if name == "complement":
            self.require_arity(name, args, 1)
            return Complement(args[0])
        raise AssertionError(name)

    def parse_arg_list(self) -> List[object]:
        args: List[object] = []
        self.skip_ws()
        if self.i < self.n and self.s[self.i] == ")":
            self.i += 1
            return args
        while True:
            args.append(self.parse_expr())
            self.skip_ws()
            if self.i < self.n and self.s[self.i] == ",":
                self.i += 1
                continue
            self.expect_close()
            return args

    def expect_close(self) -> None:
        self.skip_ws()
        if self.i >= self.n or self.s[self.i] != ")":
            self.error("expected ')'")
        self.i += 1

    def expect_comma(self) -> None:
        self.skip_ws()
        if self.i >= self.n or self.s[self.i] != ",":
            self.error("expected ','")
        self.i += 1

    @staticmethod
    def require_arity(name: str, args: List[object], count: int) -> None:
        if len(args) != count:
            raise ParseError(f"'{name}' expects {count} argument(s), got {len(args)}")


def _class_segments(chars: List[str], error) -> List[Tuple[int, int]]:
    segments: List[Tuple[int, int]] = []
    i = 0
    while i < len(chars):
        if i + 2 < len(chars) and chars[i + 1] == "-":
            lo, hi = ord(chars[i]), ord(chars[i + 2])
            if lo > hi:
                error(f"character class range out of order: {chars[i]}-{chars[i + 2]}")
            segments.append((lo, hi))
            i += 3
        else:
            segments.append((ord(chars[i]), ord(chars[i])))
            i += 1
    return segments


# ---------------------------------------------------------------------------
# Emission
# ---------------------------------------------------------------------------


def camel(name: str) -> str:
    parts = re.split(r"[-_]", name)
    return "".join(p[:1].upper() + p[1:] for p in parts if p)


def cpp_char(ch: str) -> str:
    escapes = {"\\": "\\\\", "'": "\\'", "\n": "\\n", "\r": "\\r", "\t": "\\t", "\0": "\\0"}
    if ch in escapes:
        return f"'{escapes[ch]}'"
    code = ord(ch)
    if 0x20 <= code < 0x7F:
        return f"'{ch}'"
    return f"'\\x{code:02x}'"


def inline_include(path: str, source: str) -> str:
    """Read a %include target and return its text, minus a leading #pragma once.

    The path is resolved relative to the grammar file, so a grammar and its
    bindings can sit side by side.
    """
    resolved = os.path.join(os.path.dirname(source), path)
    with open(resolved, "r", encoding="utf-8") as handle:
        text = handle.read()
    text = text.lstrip("\n")
    if text.startswith("#pragma once"):
        text = text[len("#pragma once") :].lstrip("\n")
    return text.rstrip("\n")


class Emitter:
    def __init__(self, items: List[object], rules: List[Rule]):
        self.items = items
        self.rules = rules
        self.by_name = {r.name: r for r in rules}
        self.uses_lit = False
        self.uses_keyword = False

    # -- validation ---------------------------------------------------------
    def validate(self) -> None:
        for rule in self.rules:
            if rule.name in _CONSTRUCTORS:
                raise ParseError(f"rule '{rule.name}' collides with the '{rule.name}' constructor")
            self.validate_expr(rule)

    def validate_expr(self, rule: Rule) -> None:
        aliases: Dict[str, int] = {}
        for index, other in enumerate(self.rules):
            if other.kind == "alias":
                aliases[other.name] = index
        for node in _walk(rule.expr):
            if isinstance(node, Name):
                target = self.by_name.get(node.name)
                if target is None:
                    raise ParseError(f"rule '{rule.name}': unknown rule '{node.name}'")
                if target.kind != "alias":
                    raise ParseError(
                        f"rule '{rule.name}': struct '{node.name}' must be referenced as ref({node.name})"
                    )
                if aliases.get(node.name, len(self.rules)) >= rule.index:
                    raise ParseError(
                        f"rule '{rule.name}': alias '{node.name}' is used before it is declared"
                    )
            if isinstance(node, Ref):
                if node.name not in self.by_name:
                    raise ParseError(f"rule '{rule.name}': unknown rule '{node.name}'")

    # -- type expressions ---------------------------------------------------
    def type_of(self, node: object) -> str:
        if isinstance(node, Seq):
            if not node.elements:
                raise ParseError("empty sequence is not a valid element")
            if len(node.elements) == 1:
                return self.type_of(node.elements[0])
            return "ExpandableRule<" + ", ".join(self.type_of(e) for e in node.elements) + ">"
        if isinstance(node, Alt):
            if len(node.alternatives) == 1:
                return self.type_of(node.alternatives[0])
            return "FirstMatch<" + ", ".join(self.type_of(a) for a in node.alternatives) + ">"
        if isinstance(node, Star):
            return f"StarArray<{self.type_of(node.element)}>"
        if isinstance(node, Plus):
            inner = self.type_of(node.element)
            return f"ExpandableRule<{inner}, StarArray<{inner}>>"
        if isinstance(node, Opt):
            return f"Optional<{self.type_of(node.element)}>"
        if isinstance(node, Lit):
            if not node.text:
                raise ParseError("empty literal is not a valid terminal")
            if len(node.text) == 1:
                return f"Char::Exact<{cpp_char(node.text)}>"
            self.uses_lit = True
            return "Lit<" + ", ".join(cpp_char(c) for c in node.text) + ">"
        if isinstance(node, Kw):
            if not node.text:
                raise ParseError("empty keyword is not a valid terminal")
            self.uses_lit = True
            self.uses_keyword = True
            return "Keyword<" + ", ".join(cpp_char(c) for c in node.text) + ">"
        if isinstance(node, Exact):
            return f"Char::Exact<{cpp_char(node.char)}>"
        if isinstance(node, Range):
            return f"Char::Range<{cpp_char(node.lo)}, {cpp_char(node.hi)}>"
        if isinstance(node, Union):
            if len(node.parts) == 1:
                return self.type_of(node.parts[0])
            return "Char::Union<" + ", ".join(self.type_of(p) for p in node.parts) + ">"
        if isinstance(node, Complement):
            return f"Char::Complement<{self.type_of(node.element)}>"
        if isinstance(node, Class):
            return self.class_of(node)
        if isinstance(node, Name):
            return camel(node.name)
        if isinstance(node, Ref):
            return f"RuleRef<{camel(node.name)}>"
        if isinstance(node, Eof):
            return "Eof"
        if isinstance(node, Directive):
            return f"Signal<nota::{directive_tag(node.name, node.args)}>"
        if isinstance(node, Region):
            # A region is sugar for a begin/end marker pair around its body.
            begin = f"Signal<nota::{directive_tag(node.name, ['begin'] + node.args)}>"
            end = f"Signal<nota::{directive_tag(node.name, ['end'] + node.args)}>"
            if isinstance(node.body, Seq):
                inner = [self.type_of(element) for element in node.body.elements]
            else:
                inner = [self.type_of(node.body)]
            parts = [begin, *inner, end]
            return parts[0] if len(parts) == 1 else "ExpandableRule<" + ", ".join(parts) + ">"
        raise AssertionError(node)

    def class_of(self, node: Class) -> str:
        parts = []
        for lo, hi in node.segments:
            if lo == hi:
                parts.append(f"Char::Exact<{cpp_char(chr(lo))}>")
            else:
                parts.append(f"Char::Range<{cpp_char(chr(lo))}, {cpp_char(chr(hi))}>")
        base = parts[0] if len(parts) == 1 else "Char::Union<" + ", ".join(parts) + ">"
        return f"Char::Complement<{base}>" if node.negated else base

    # -- rendering ----------------------------------------------------------
    def render(self, namespace: str, source: str) -> str:
        bodies: Dict[int, str] = {rule.index: self.type_of(rule.expr) for rule in self.rules}

        lines: List[str] = []
        lines.append("#pragma once")
        lines.append("")
        lines.append(f"// Generated from {source} by tools/grammar_gen.py. Do not edit.")
        lines.append("")
        lines.append('#include "parser3/core/grammar_element.h"')
        lines.append('#include "parser3/core/grammar_stack.h"')
        lines.append('#include "parser3/core/semantics.h"')
        lines.append("")
        lines.append(f"namespace {namespace} {{")
        lines.append("")

        if self.uses_lit or self.uses_keyword:
            lines.append("// An exact byte sequence terminal.")
            lines.append("template <char... Chars>")
            lines.append("using Lit = ExpandableRule<Char::Exact<Chars>...>;")
            lines.append("")
        if self.uses_keyword:
            lines.append("// Consumes nothing when the next byte (if any) is not an identifier-continue byte.")
            lines.append("struct NotNameChar : GrammarFrame<NotNameChar> {")
            lines.append("    static constexpr bool Nullable = true;")
            lines.append("")
            lines.append("    bool Feed(ContextView ctx) {")
            lines.append("        if (ctx.chars.Empty()) {")
            lines.append("            return ctx.chars.Finished() ? ctx.stack.Reduce(this, ctx) : true;")
            lines.append("        }")
            lines.append("        const unsigned c = static_cast<unsigned char>(ctx.chars.Front());")
            lines.append("        const bool name = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||")
            lines.append("                          c == '_' || c == '$';")
            lines.append("        return name ? ctx.stack.Throw(ctx) : ctx.stack.Reduce(this, ctx);")
            lines.append("    }")
            lines.append("};")
            lines.append("")
            lines.append("// A keyword: exact bytes bounded by a non-name byte.")
            lines.append("template <char... Chars>")
            lines.append("using Keyword = ExpandableRule<Lit<Chars...>, NotNameChar>;")
            lines.append("")

        structs = [r for r in self.rules if r.kind == "struct"]
        includes = [item for item in self.items if isinstance(item, Include)]
        lines.append("// Forward declarations (struct rules may reference each other through ref()).")
        for rule in structs:
            lines.append(f"struct {camel(rule.name)};")
        lines.append("")

        tags: List[str] = []

        def add_tag(tag: str) -> None:
            if tag not in tags:
                tags.append(tag)

        for rule in self.rules:
            for node in _walk(rule.expr):
                if isinstance(node, Directive):
                    add_tag(directive_tag(node.name, node.args))
                elif isinstance(node, Region):
                    add_tag(directive_tag(node.name, ["begin"] + node.args))
                    add_tag(directive_tag(node.name, ["end"] + node.args))
        if tags:
            lines.append("// Semantic tags. Specialize SignalAction<nota::Tag> for each element.")
            lines.append("namespace nota {")
            for tag in tags:
                lines.append(f"struct {tag};")
            lines.append("} // namespace nota")
            lines.append("")

        # Author-owned bindings are inlined at global scope so their SignalAction
        # specializations are legal.  Inlining keeps the generated header
        # self-contained and leaves the bindings with no other consumer.
        if includes:
            lines.append(f"}} // namespace {namespace}")
            lines.append("")
            for include in includes:
                lines.append(inline_include(include.path, source))
                lines.append("")
            lines.append(f"namespace {namespace} {{")
            lines.append("")

        lines.append("// Rules, in declaration order.")
        for rule in self.rules:
            base = bodies[rule.index]
            if rule.kind == "alias":
                lines.append(f"using {camel(rule.name)} = {base};")
            else:
                lines.append(f"struct {camel(rule.name)} : {base} {{}};")
        lines.append("")
        lines.append(f"}} // namespace {namespace}")
        lines.append("")
        return "\n".join(lines)


def directive_tag(name: str, args: List[str]) -> str:
    parts = [camel(name)] + [camel(arg) for arg in args]
    return "_".join(p for p in parts if p)


def _walk(node: object):
    yield node
    if isinstance(node, Seq):
        for child in node.elements:
            yield from _walk(child)
    elif isinstance(node, Alt):
        for child in node.alternatives:
            yield from _walk(child)
    elif isinstance(node, (Star, Plus, Opt, Complement)):
        yield from _walk(node.element)
    elif isinstance(node, Union):
        for child in node.parts:
            yield from _walk(child)
    elif isinstance(node, Region):
        yield from _walk(node.body)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input", help="grammar description file")
    parser.add_argument("-o", "--output", help="output header (default: stdout)")
    parser.add_argument("-n", "--namespace", default="vera::parser3::generated", help="C++ namespace for the rules")
    parser.add_argument("--clang-format", action="store_true", help="run clang-format-14 on the result")
    args = parser.parse_args(argv)

    with open(args.input, "r", encoding="utf-8") as handle:
        text = handle.read()

    try:
        items = Parser(text, args.input).parse()
        rules = [item for item in items if isinstance(item, Rule)]
        if not rules:
            raise ParseError(f"{args.input}: no rules found")
        emitter = Emitter(items, rules)
        emitter.validate()
        result = emitter.render(args.namespace, args.input)
    except ParseError as error:
        print(f"grammar_gen: {error}", file=sys.stderr)
        return 1

    if args.clang_format:
        import subprocess

        try:
            result = subprocess.run(
                ["clang-format-14"], input=result, capture_output=True, text=True, check=True
            ).stdout
        except (FileNotFoundError, subprocess.CalledProcessError):
            print("grammar_gen: clang-format-14 not available; writing unformatted output", file=sys.stderr)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as handle:
            handle.write(result)
    else:
        sys.stdout.write(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
