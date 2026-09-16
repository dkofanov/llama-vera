# llama-vera

`llama-vera` drives a llama.cpp completion constrained by parser3. It owns the
VERA grammar it is built on — `grammars/full.grammar` (syntax) and
`grammars/full_sem.grammar` with its `grammars/full_semantics.h` bindings
(syntax plus class semantics) — and generates their headers with parser3's
`tools/grammar_gen.py` into `parser3-generated/`.

```
src/parser3_vera.{h,cpp}       a parser3 instance for VERA (no llama.cpp)
src/llama_vera_sampler.{h,cpp} the llama.cpp sampler adapter over it
src/main.cpp                   the driver
grammars/                      the grammar sources + semantic bindings
tests/                         parser3_vera, class semantics, sampler
benchmarks/                    full-grammar and fork benchmarks
```

## Constraint modes

| mode | flag | engine |
|---|---|---|
| default | *(none)* | parser3 `full_sem` + class semantics |
| parser3 syntax | `--no-sem` | parser3 `full`, no semantics |
| built-in GBNF | `--gbnf` | llama.cpp GBNF, the bundled full grammar |
| unconstrained | `--no-grammar` | none |

```sh
./build/examples/llama-vera/llama-vera -m model.gguf -p 'class Point { x: int; } class ' -n 40
```

## Semantic model

A demo "class layer": a module is a sequence of class declarations, each field
typed by a builtin or by an already-declared class; duplicate class names,
duplicate field names, and forward- or self-type references are rejected.
**These rules are fictional** — they exist to exercise parser3's semantics hooks,
not as a requirement of any real language.

## Containment

With the syntax-only grammar the model freely emits invalid programs; with the
class semantics (the default) the rejected token fails the match, so the
constraint picks a legal alternative *during* generation rather than generating
and then filtering. Same prompt and seed, `llama-vera -n 40`:

| request | syntax only (`--no-sem`) | class semantics (default) |
|---|---|---|
| `a class A whose field has type B, and class B is declared after A` | `class A { field: B; }` — forward reference to a later `B` | `class A { field: string; }` — undeclared type rejected |
| `a function returning an undefined type` | `function returnUndefined() : undefined { … }` | `function returnUndefined() : int { … }` — non-viable type rejected |
| `a class with two fields named value` | three closed duplicate `class MyClass { value: string; }` | the second `class MyClass` can never close |

## Generation overhead

Each generated token is constrained against the whole ~152k-token vocab, so the
grammar step dominates the decode. The representative case is a request the
constraints must *reject*: the sampler can never accept early, so it walks the
whole sweep and rewinds.

| rejected request (prompt) | parser3 syntax (`--no-sem`) | parser3 + class semantics (default) | llama.cpp GBNF (`--gbnf`) |
|---|---|---|---|
| duplicate field (`a class with two fields named value`) | 23.9 | 39.8 | 28.4 |
| forward reference (`a class that references a class declared later`) | 23.2 | 35.9 | 31.2 |
| undefined return type (`a function returning an undefined type`) | 67.1 | 76.3 | 129.8 |

milliseconds per token for the grammar step, 40 tokens per prompt, measured over
these grammars and this engine with an instrumented two-grammar run (the driver
itself prints only the generated text). Each run is truncated at the `-n 40` cap
rather than ending on EOG: an unsatisfiable request gives the model no accepting
place to stop, so the result is a broken, unclosed prefix.

parser3 is ~1.2–1.9x faster than GBNF. The class semantics adds ~1.1–1.7x in the
sweep (~3.3x on the accepted-path feed, where no vocab walk amortizes it), so
parser3+semantics vs GBNF is ~0.6–1.4x, request-dependent. All dwarf the
~5 ms/token decode: the cost is the ~152k-candidate sweep, not matching speed.
