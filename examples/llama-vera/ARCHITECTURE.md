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

Each generated token is constrained against the whole vocabulary (~152k tokens
for Qwen2.5-0.5B): the sampler tests every candidate piece against the current
parser state — a `Fork` + `Feed` each, minus the pieces pruned by the first-byte
mask — so one token is one sweep over the vocab. The representative case is a
request the constraints must *reject*: the sampler can never accept early, so it
walks the whole sweep and rewinds.

| rejected request (prompt) | parser3 syntax (`--no-sem`) | parser3 + class semantics (default) | llama.cpp GBNF (`--gbnf`) |
|---|---|---|---|
| duplicate field (`a class with two fields named value`) | 24.0 | 39.5 | 29.3 |
| forward reference (`a class that references a class declared later`) | 21.9 | 35.8 | 28.7 |
| undefined return type (`a function returning an undefined type`) | 67.2 | 76.1 | 130.3 |

milliseconds per token for the grammar step, 40 tokens per prompt, measured over
these grammars and this engine with an instrumented two-grammar run (the driver
itself prints only the generated text). Each run is truncated at the `-n 40` cap
rather than ending on EOG: an unsatisfiable request gives the model no accepting
place to stop, so the result is a broken, unclosed prefix.

parser3 is ~1.2–1.9x faster than GBNF. The class semantics adds ~1.1–1.7x in the
sweep (~3.3x on the accepted-path feed, where no vocab walk amortizes it), so
parser3+semantics vs GBNF is ~0.6–1.4x, request-dependent.

The cost per token is set by the vocabulary size, not by parser3's matching
throughput: ~152k candidate trials per token (10^6–10^7 ns) dwarf the
~5 ms/token decode, and are many orders of magnitude above the 5–134 ns/byte
that `parser3-bench` measures. A faster `Feed` barely moves this number; pruning
the candidate set (first-byte mask, deeper tries) is what does.

## Benchmarks

Release (`-O2 -DNDEBUG`), i7-13700, g++ 11.4, Ubuntu 22.04; machine-specific.

Both benchmarks parse a generated VERA program of `units` function+class pairs:
`function fnN(a: int, b: number): int { let x: int = a + b * 2; if (x > 0) { return x; } else { return 0; } }`
followed by `class CN { f: int; g: string; }`, fed in fixed `chunk`-byte pieces.

`llama-vera-full-bench 300 40 64` — 300 units, 43280 bytes, run 40 times, syntax
grammar only:

```
134.06 ns/byte   7.5 MB/s
```

`llama-vera-fork-bench 50 10 64` — 50 units, 7130 bytes. At each 64-byte piece it
forks the current context `forks` times, feeds the piece into every copy, commits
the first accepted, and discards the rest (the sampler's per-candidate trial);
`forks=1` is the no-fork baseline:

| forks | ns/byte | MB/s | overhead |
|---|---|---|---|
| 1 | 136.4 | 7.3 | 1.00x |
| 2 | 291.9 | 3.4 | 2.14x |
| 3 | 450.7 | 2.2 | 3.30x |

The fork-copy line times `Fork()` alone at a half-parsed state (0.08 µs/fork),
isolating the copy from parsing.
