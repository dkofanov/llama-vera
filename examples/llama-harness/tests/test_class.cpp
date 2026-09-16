// Exercises the class semantic constraints of grammars/full_sem.grammar:
// duplicate class/field names are rejected, and type references must resolve,
// character by character, to a declared type (built-in or an earlier class).
// Self-reference and forward references are rejected because a class name is
// only declared when its definition closes.

#include "parser3/core/context.h"
#include "parser3/core/context_view.h"
#include "parser3-generated/full_sem_grammar.h"

#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

using namespace vera::parser3;
using vera::parser3::full_sem::Root;

static int failures = 0;

// True when the whole program parses to completion (including EOF).
static bool accepts(const std::string &text) {
    Context ctx(1 << 14, std::in_place_type<Root>, std::make_unique<ClassSemantics>());
    ctx.chars.Append(text.data(), text.size());
    ContextView view(ctx);
    if (!ctx.stack.Head()->Feed(view)) {
        return false;
    }
    if (ctx.stack.Head() == nullptr) {
        return true;
    }
    ctx.chars.Finish();
    return ctx.stack.Head()->Feed(view) && ctx.stack.Head() == nullptr;
}

static void expect(const char *name, const std::string &text, bool want) {
    const bool got = accepts(text);
    if (got == want) {
        std::printf("PASS  %-28s %s\n", name, want ? "accept" : "reject");
    } else {
        ++failures;
        std::printf("FAIL  %-28s got %s\n", name, got ? "accept" : "reject");
    }
}

int main() {
    expect("empty module", "", true);
    expect("single class", "class Node { value: int; }", true);
    expect("builtin and class types", "class Node { value: number; name: string; }", true);
    expect("reference earlier class", "class Node { value: int; } class Other { n: Node; }", true);
    expect("same field in different classes", "class A { x: int; } class B { x: number; }", true);
    expect("same fields across classes", "class A { x: int; y: int; } class B { x: string; y: boolean; }", true);
    expect("self reference", "class Node { next: Node; }", false);
    expect("forward reference", "class A { b: B; } class B { }", false);
    expect("duplicate class", "class A {} class A {}", false);
    expect("duplicate field", "class A { x: int; x: number; }", false);
    expect("non-viable type", "class A { x: intt; }", false);
    expect("unknown type", "class A { x: Zed; }", false);

    // Non-trivial multi-class programs: a class may reference any earlier class.
    expect("chain of references", "class A { a: int; } class B { b: A; } class C { c: B; x: A; }", true);
    expect("segment of points", "class Point { x: number; y: number; } class Segment { a: Point; b: Point; }", true);
    expect("three-level graph",
           "class A { n: int; } class B { a: A; label: string; } class C { b: B; a: A; } class D { c: C; }", true);
    // ... but never a class that is not yet declared.
    expect("mutual forward refs", "class A { b: B; } class B { a: A; }", false);
    expect("self ref in field", "class A { x: A; }", false);
    expect("forward ref to third", "class A { c: C; } class B {} class C {}", false);
    expect("forward ref after a valid one", "class A { a: int; } class B { x: C; } class C {}", false);

    if (failures == 0) {
        std::printf("\nall class tests passed\n");
        return 0;
    }
    std::printf("\n%d test(s) failed\n", failures);
    return 1;
}
