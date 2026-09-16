# parser3 architecture

parser3 is a header-only, incremental, byte-level PEG engine used to constrain
token sampling. For testing it drives a demo "class layer" model: a module is a
sequence of class declarations, each field typed by a builtin or by an
already-declared class; duplicate class names, duplicate field names, and
forward- or self-type references are rejected. **These rules are fictional** —
they exist only to exercise the semantics hooks, not as a requirement of any
real language.

## Principles

**Grammar stack.** A grammar is a C++ type; matching is a control stack — frames
are pushed and reduced on a bump-allocated stack, and backtracking is a LIFO
rewind to the nearest checkpoint. Nothing is interpreted at runtime.

**Grammar description.** That C++ type is generated at compile time by
`tools/grammar_gen.py` from a local element DSL; sugar (`|`, `*`, `+`, `?`,
literals, character classes, `ref`) is a documented desugaring. Rules are emitted
in file order; recursion is spelled out as `struct` + `ref(...)`. Grammar
headers are build artifacts: the build generates them from the `.grammar`
sources under the `parser3-generated/` include prefix, and a grammar may
`%include` hand-written bindings that are inlined into its header. Nothing
generated is committed or installed.

**Semantics mechanism.** Semantics are an extension of the same description, not
a second pass. A `@`-marked element is an ordinary grammar element:
`@name(args)` is a zero-width marker, `@name(args){ body }` is a region (sugar
for `@name(begin,args)…@name(end,args)`), and each invokes a
`SignalAction<Tag>::Feed` binding whose false is a *failed match*, so it
backtracks like a terminal. The author's `SemanticState` lives on the `Context`
(cloned on `Fork`, adopted on `Commit`) and is rewound by the grammar's own
checkpoints — a discarded branch has no semantic effect.

**Containment.** With the syntax-only grammar the model freely emits invalid
programs; with `--sem` the rejected token fails the match, so the constraint
picks a legal alternative *during* generation rather than generating and then
filtering. Same prompt and seed, `llama-assurance -n 40`:

| request | syntax only | with `--sem` |
|---|---|---|
| `a class A whose field has type B, and class B is declared after A` | `class A { field: B; }` — forward reference to a later `B` | `class A { field: string; }` — undeclared type rejected |
| `a function returning an undefined type` | `function returnUndefined() : undefined { … }` | `function returnUndefined() : int { … }` — non-viable type rejected |
| `a class with two fields named value` | three closed duplicate `class MyClass { value: string; }` | the second `class MyClass` can never close |

**Generation overhead.** Each generated token is constrained against the whole
~152k-token vocab, so the grammar step dominates the decode. The representative
case is a request the constraints must *reject*: the sampler can never accept
early, so it walks the whole sweep and rewinds. Measured with `llama-assurance`
(Release, Qwen2.5-0.5B, `-n 40`): the lockstep run reports parser3 vs llama GBNF
(both accept the syntax), `--sem` reports parser3 with the class semantics (which
reject the request and force the model onto a legal alternative).

| rejected request (prompt) | completion at 40 tokens | parser3 | parser3 + class semantics | llama GBNF |
|---|---|---|---|---|
| duplicate field (`a class with two fields named value`) | cap, no EOG; emits `class MyClass { value: string; } class MyClass { value: string; // …`, second class never closes | 23.9 | 39.8 | 28.4 |
| forward reference (`a class that references a class declared later`) | cap, no EOG; `class Z { var …` never closes | 23.2 | 35.9 | 31.2 |
| undefined return type (`a function returning an undefined type`) | cap, no EOG; repeats the function, last one cut mid-declaration | 67.1 | 76.3 | 129.8 |

milliseconds per token for the grammar step. Each run is truncated at the
`-n 40` cap rather than ending on EOG: an unsatisfiable request gives the model
no accepting place to stop, so it keeps emitting (a duplicate class, a repeated
function) and the result is a broken, unclosed prefix, not a completed program.

parser3 is ~1.2–1.9x faster than GBNF. Class semantics adds ~1.1–1.7x in the
sweep (~3.3x on the accepted-path feed, where no vocab walk amortizes it), so
parser3+sem vs GBNF is ~0.6–1.4x, request-dependent. All dwarf the ~5 ms/token
decode: the cost is the ~152k-candidate sweep, not matching speed.
