# parser3 architecture

parser3 is a header-only, incremental, byte-level PEG engine used to constrain
token sampling. It has no language of its own: a grammar is a C++ type, and
semantic constraints are attached to it through the hooks described below. The
VERA class-layer grammar, its bindings, and the generation measurements live with
the example consumer — see `examples/llama-vera/ARCHITECTURE.md`.

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
generated is committed or installed. The grammars the library ships are its own
test fixtures (`tests/context.grammar`, `tests/signal.grammar`); application
grammars such as the VERA `full` / `full_sem` pair live with their consumer.

**Semantics mechanism.** Semantics are an extension of the same description, not
a second pass. A `@`-marked element is an ordinary grammar element:
`@name(args)` is a zero-width marker, `@name(args){ body }` is a region (sugar
for `@name(begin,args)…@name(end,args)`), and each invokes a
`SignalAction<Tag>::Feed` binding whose false is a *failed match*, so it
backtracks like a terminal. The author's `SemanticState` lives on the `Context`
(cloned on `Fork`, adopted on `Commit`) and is rewound by the grammar's own
checkpoints — a discarded branch has no semantic effect.
