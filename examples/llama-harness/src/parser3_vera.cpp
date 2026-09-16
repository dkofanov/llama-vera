#include "parser3_vera.h"

// parser3-backed VERA checker.  In syntax mode the parse is the generated full
// grammar; in semantics mode it is the full_sem grammar with its ClassSemantics
// bindings (duplicate class/field names and type references that must resolve to
// a previously declared class).

#include "parser3/core/context.h"
#include "parser3/core/context_view.h"
#include "parser3/examples/full_grammar.h"
#include "parser3/examples/full_sem_grammar.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace vera::parser3;

namespace {
constexpr std::size_t kStackSlots = 1 << 16;
}

struct parser3_vera::impl {
    vera_mode mode;
    Context ctx;
    std::vector<diagnostic> diags;
    std::size_t piece_index = 0;
    bool failed = false;

    explicit impl(vera_mode m) : mode(m), ctx(make_context(m)) {}

    static Context make_context(vera_mode m) {
        if (m == vera_mode::syntax) {
            return Context(kStackSlots, std::in_place_type<full::Root>);
        }
        return Context(kStackSlots, std::in_place_type<full_sem::Root>, std::make_unique<ClassSemantics>());
    }

    impl(const impl &other)
        : mode(other.mode), ctx(other.ctx.DeepCopy()), diags(other.diags), piece_index(other.piece_index),
          failed(other.failed) {}

    // Mark the current input as rejected.  Only the semantic mode records a
    // diagnostic; syntax mode has nothing to report.
    void record(std::size_t index) {
        if (mode == vera_mode::semantics) {
            const ClassSemantics *state = ctx.stack.SemanticsAs<ClassSemantics>();
            diags.push_back(
                {index, state ? state->violated_name() : std::string{}, state ? state->violated_scope() : std::string{}});
        }
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
        const bool ok = ctx.stack.Head() != nullptr && ctx.stack.Head()->Feed(view) && ctx.stack.Head() == nullptr;
        if (!ok) {
            record(piece_index);
        }
    }
};

parser3_vera::parser3_vera() : parser3_vera(vera_mode::semantics) {}

parser3_vera::parser3_vera(vera_mode mode) : p_(new impl(mode)) {}

parser3_vera::~parser3_vera() = default;

parser3_vera::parser3_vera(const parser3_vera &other) : p_(new impl(*other.p_)) {}

parser3_vera &parser3_vera::operator=(const parser3_vera &other) {
    if (this != &other) {
        p_.reset(new impl(*other.p_));
    }
    return *this;
}

void parser3_vera::feed(const std::string &piece) {
    p_->feed(piece);
}

void parser3_vera::finalize() {
    p_->finalize();
}

bool parser3_vera::would_introduce_diagnostic(const std::string &piece, bool finalize_candidate) const {
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

const std::vector<diagnostic> &parser3_vera::diagnostics() const {
    return p_->diags;
}

bool parser3_vera::failed() const {
    return p_->failed;
}

vera_mode parser3_vera::mode() const {
    return p_->mode;
}

parser3_vera parser3_vera::clone() const {
    return parser3_vera(*this);
}

void parser3_vera::reset() {
    p_.reset(new impl(p_->mode));
}

std::vector<diagnostic> vera_diagnostics(const std::string &text) {
    parser3_vera checker;
    checker.feed(text);
    checker.finalize();
    return checker.diagnostics();
}
