// Exercises inline @notation frames: the grammar accumulates an identifier one
// character at a time (Signal::Char), rejecting as soon as the prefix can no
// longer become one of the allowed names, and requires a complete allowed name
// at the end (Signal::End).

#include "parser3/core/context.h"
#include "parser3/core/context_view.h"
#include "parser3-generated/signal_grammar.h"

#include <cstddef>
#include <cstdio>
#include <set>
#include <string>
#include <utility>

using namespace vera::parser3;
using vera::parser3::signal_demo::Root;

static int failures = 0;

static void check(bool condition, const char *message) {
    if (condition) {
        std::printf("PASS  %s\n", message);
    } else {
        ++failures;
        std::printf("FAIL  %s\n", message);
    }
}

namespace {

thread_local std::string g_name;

const std::set<std::string> &names() {
    static const std::set<std::string> allowed = {"foo", "bar", "foobar"};
    return allowed;
}

bool viable(const std::string &prefix) {
    for (const std::string &name : names()) {
        if (name.size() >= prefix.size() && name.compare(0, prefix.size(), prefix) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

namespace vera::parser3 {

template <>
struct SignalAction<signal_demo::nota::Begin> {
    static bool Feed(ContextView) {
        g_name.clear();
        return true;
    }
};

template <>
struct SignalAction<signal_demo::nota::Char> {
    static bool Feed(ContextView ctx) {
        g_name.push_back(ctx.chars.ByteAt(ctx.chars.CurrentOffset() - 1));
        return viable(g_name);
    }
};

template <>
struct SignalAction<signal_demo::nota::End> {
    static bool Feed(ContextView) {
        return names().count(g_name) != 0;
    }
};

} // namespace vera::parser3

namespace {

Context Make() {
    return Context(1 << 10, std::in_place_type<Root>);
}

bool Feed(Context &ctx, const char *text, std::size_t size) {
    ctx.chars.Append(text, size);
    if (ctx.stack.Head() == nullptr) {
        return true;
    }
    return ctx.stack.Head()->Feed(ContextView{ctx});
}

bool Finish(Context &ctx) {
    ctx.chars.Finish();
    if (ctx.stack.Head() == nullptr) {
        return true;
    }
    return ctx.stack.Head()->Feed(ContextView{ctx}) && ctx.stack.Head() == nullptr;
}

} // namespace

int main() {
    {
        Context ctx = Make();
        check(Feed(ctx, "foo ", 4), "accepts 'foo '");
        check(g_name == "foo", "accumulated the name character by character");
        check(Finish(ctx), "accepts EOF after 'foo '");
    }
    {
        Context ctx = Make();
        check(Feed(ctx, "foobar ", 7), "accepts 'foobar '");
    }
    {
        Context ctx = Make();
        check(!Feed(ctx, "foox ", 5), "rejects when the prefix diverges ('foox')");
    }
    {
        Context ctx = Make();
        check(!Feed(ctx, "baz ", 4), "rejects when the prefix diverges ('baz')");
    }
    {
        Context ctx = Make();
        check(Feed(ctx, "fo", 2), "accepts a viable partial prefix ('fo')");
        check(Feed(ctx, "o ", 2), "accepts the completed prefix ('foo ')");
        check(Finish(ctx), "accepts EOF after the split feed");
    }
    {
        Context ctx = Make();
        check(Feed(ctx, "fo", 2), "accepts viable partial 'fo'");
        check(!Finish(ctx), "rejects an incomplete name at EOF ('fo')");
    }
    {
        Context ctx = Make();
        check(!Feed(ctx, "foob ", 5), "rejects a viable but non-allowed name ('foob')");
    }

    if (failures == 0) {
        std::printf("\nall signal tests passed\n");
        return 0;
    }
    std::printf("\n%d test(s) failed\n", failures);
    return 1;
}
