#include "vera_analyzer.h"

// parser3-backed replacement for the previous hand-written lexer/parser/checker.
// The whole parse and the semantic constraints (duplicate class/field names,
// duplicate names in the module scope, and type references that must resolve to
// a previously declared class) are implemented by the generated full_sem
// grammar and its ClassSemantics bindings. The public API is unchanged.

#include "parser3/core/context.h"
#include "parser3/core/context_view.h"
#include "parser3/examples/full_sem_grammar.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace vera::parser3;
using vera::parser3::full_sem::Root;

namespace {
constexpr std::size_t kStackSlots = 1 << 16;
}

struct vera_checker::impl {
    Context ctx;
    std::vector<diagnostic> diags;
    std::size_t piece_index = 0;
    bool failed = false;

    impl() : ctx(kStackSlots, std::in_place_type<Root>, std::make_unique<ClassSemantics>()) {}

    impl(const impl &other)
        : ctx(other.ctx.DeepCopy()), diags(other.diags), piece_index(other.piece_index), failed(other.failed) {}

    // Record the current violation (name/scope from the semantics) and stop.
    void record(std::size_t index) {
        const ClassSemantics *state = ctx.stack.SemanticsAs<ClassSemantics>();
        diags.push_back(
            {index, state ? state->violated_name() : std::string{}, state ? state->violated_scope() : std::string{}});
        failed = true;
    }

    void feed(const std::string &piece) {
        if (failed || ctx.stack.Head() == nullptr) {
            return;
        }
        ctx.chars.Append(piece.data(), piece.size());
        ContextView view(ctx);
        const bool ok = ctx.stack.Head() != nullptr && ctx.stack.Head()->Feed(view);
        if (!ok) {
            record(piece_index);
        }
        ++piece_index;
    }

    void finalize() {
        if (failed || ctx.stack.Head() == nullptr) {
            return;
        }
        ctx.chars.Finish();
        ContextView view(ctx);
        const bool ok =
            ctx.stack.Head() != nullptr && ctx.stack.Head()->Feed(view) && ctx.stack.Head() == nullptr;
        if (!ok) {
            record(piece_index);
        }
    }
};

vera_checker::vera_checker() : p_(new impl()) {}

vera_checker::~vera_checker() = default;

vera_checker::vera_checker(const vera_checker &other) : p_(new impl(*other.p_)) {}

vera_checker &vera_checker::operator=(const vera_checker &other) {
    if (this != &other) {
        p_.reset(new impl(*other.p_));
    }
    return *this;
}

void vera_checker::feed(const std::string &piece) {
    p_->feed(piece);
}

void vera_checker::finalize() {
    p_->finalize();
}

bool vera_checker::would_introduce_diagnostic(const std::string &piece, bool finalize_candidate) const {
    if (p_->failed) {
        return true;
    }
    Context probe = p_->ctx.Fork();
    if (probe.stack.Head() == nullptr) {
        return true;
    }
    probe.chars.Append(piece.data(), piece.size());
    ContextView view(probe);
    if (!probe.stack.Head()->Feed(view)) {
        return true;
    }
    if (finalize_candidate) {
        probe.chars.Finish();
        if (probe.stack.Head() == nullptr) {
            return false;
        }
        if (!probe.stack.Head()->Feed(view) || probe.stack.Head() != nullptr) {
            return true;
        }
    }
    return false;
}

const std::vector<diagnostic> &vera_checker::diagnostics() const {
    return p_->diags;
}

vera_checker vera_checker::clone() const {
    return vera_checker(*this);
}

void vera_checker::reset() {
    p_.reset(new impl());
}

std::vector<diagnostic> vera_analyze(const std::string &text) {
    vera_checker checker;
    checker.feed(text);
    checker.finalize();
    return checker.diagnostics();
}
