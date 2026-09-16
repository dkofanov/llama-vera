#pragma once

#include "parser3/core/char_stream.h"
#include "parser3/core/context_view.h"
#include "parser3/core/grammar_stack.h"
#include "parser3/core/semantics.h"

#include <cstddef>
#include <memory>
#include <utility>

namespace vera::parser3 {

// Owning parser state. A plain struct: read it through a ContextView. Fork
// copies the state so a candidate can be tested and then committed or dropped;
// grammar trials still rewind in place inside the fork.
//
// An optional SemanticState can be attached; when present it is cloned on Fork,
// adopted on Commit, and rewound by the grammar's backtracking. When absent the
// parser is purely syntactic and nothing semantic is touched.
struct Context {
    CharStream chars;
    GrammarStack stack;
    std::unique_ptr<SemanticState> semantics;

    template <class Root>
    Context(std::size_t stackCapacity, std::in_place_type_t<Root> tag, std::unique_ptr<SemanticState> state = nullptr)
        : chars(), stack(stackCapacity, tag), semantics(std::move(state)) {
        Bind();
    }

    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    Context(Context &&) = default;
    Context &operator=(Context &&) = default;

    Context Fork() const {
        Context out(chars.Fork(), stack.Clone());
        out.semantics = semantics ? semantics->Clone() : nullptr;
        out.Bind();
        return out;
    }

    // An independent copy, safe to advance and commit without affecting the
    // original (unlike a fork, which shares the committed input store).
    Context DeepCopy() const {
        Context out(chars.DeepCopy(), stack.Clone());
        out.semantics = semantics ? semantics->Clone() : nullptr;
        out.Bind();
        return out;
    }

    // Adopt a fork: append its speculative bytes to the shared store and take
    // over its cursor, frames and semantics. The fork is left moved-from.
    void Commit(Context &&fork) {
        chars.CommitFrom(std::move(fork.chars));
        stack = std::move(fork.stack);
        semantics = std::move(fork.semantics);
        Bind();
    }

  private:
    Context(CharStream chars, GrammarStack stack) : chars(std::move(chars)), stack(std::move(stack)) {}

    void Bind() {
        stack.SetSemantics(semantics.get());
    }
};

inline ContextView::ContextView(Context &context) : chars(context.chars), stack(context.stack) {}

} // namespace vera::parser3
