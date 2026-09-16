#include "parser3/core/context.h"
#include "parser3/core/context_view.h"
#include "parser3-generated/context_grammar.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

using namespace vera::parser3;
using vera::parser3::context::Root;

static int failures = 0;

static void check(bool condition, const char *message) {
    if (condition) {
        std::printf("PASS  %s\n", message);
    } else {
        ++failures;
        std::printf("FAIL  %s\n", message);
    }
}

// Feed `text`; if the parse is not complete, mark EOF and feed once more.
static bool finish(Context &ctx, const char *text, std::size_t size) {
    ContextView view(ctx);
    ctx.chars.Append(text, size);
    bool ok = ctx.stack.Head()->Feed(view);
    if (!ok) {
        return false;
    }
    if (ctx.stack.Head() == nullptr) {
        return true;
    }
    ctx.chars.Finish();
    ok = ctx.stack.Head()->Feed(view);
    return ok && ctx.stack.Head() == nullptr;
}

int main() {
    const std::string doc = "function foo(): int {}\n";
    const std::size_t half = doc.size() / 2;

    Context base(1 << 16, std::in_place_type<Root>);
    {
        ContextView view(base);
        base.chars.Append(doc.data(), half);
        base.stack.Head()->Feed(view);
    }
    const std::size_t cursor = base.chars.CurrentOffset();

    // A discarded fork leaves the base untouched.
    {
        Context trial = base.Fork();
        check(finish(trial, doc.data() + half, doc.size() - half), "fork completes the document");
        check(base.chars.CurrentOffset() == cursor, "discarding a fork leaves the base cursor");
        check(base.stack.Head() != nullptr, "discarding a fork leaves the base unfinished");
    }

    // A forbidden fork is rejected and does not disturb the base.
    {
        Context trial = base.Fork();
        ContextView view(trial);
        trial.chars.Append("}", 1);
        check(!trial.stack.Head()->Feed(view), "fork rejects forbidden input");
        check(base.chars.CurrentOffset() == cursor, "rejected fork leaves the base cursor");
    }

    // A deep copy is independent: advancing and finishing it leaves the original
    // at its pre-copy state (a shared-store fork could not do this).
    {
        Context base2(1 << 16, std::in_place_type<Root>);
        {
            ContextView view(base2);
            base2.chars.Append(doc.data(), half);
            base2.stack.Head()->Feed(view);
        }
        const std::size_t base_cursor = base2.chars.CurrentOffset();

        Context copy = base2.DeepCopy();
        check(finish(copy, doc.data() + half, doc.size() - half), "deep copy completes the document");
        check(base2.chars.CurrentOffset() == base_cursor, "deep copy leaves the original cursor");
        check(base2.stack.Head() != nullptr, "deep copy leaves the original unfinished");

        // The original can still be completed independently of the copy.
        check(finish(base2, doc.data() + half, doc.size() - half), "original still completes after deep copy");
    }

    // A committed fork replaces the base state.
    {
        Context trial = base.Fork();
        check(finish(trial, doc.data() + half, doc.size() - half), "second fork completes the document");
        base.Commit(std::move(trial));
    }
    check(base.stack.Head() == nullptr, "commit advances the base to the finished state");

    if (failures == 0) {
        std::printf("\nall context tests passed\n");
        return 0;
    }
    std::printf("\n%d test(s) failed\n", failures);
    return 1;
}
