// The analyzer is now backed by parser3's class/module semantic constraints, so
// diagnostics cover: duplicate class names (module scope), duplicate field names
// (class scope), and type references that do not resolve to a previously
// declared class. Same-name fields in different classes, and field names equal
// to a module name, are allowed. Function/parameter/local duplicates are not
// part of the class/module constraints and are not diagnosed.

#include "vera_analyzer.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

static int failures = 0;

static void expect(const char *name, const std::string &source,
                   const std::vector<std::pair<std::string, std::string>> &expected) {
    const std::vector<diagnostic> got = vera_analyze(source);
    bool ok = got.size() == expected.size();
    for (size_t i = 0; ok && i < expected.size(); ++i) {
        ok = got[i].name == expected[i].first && got[i].scope == expected[i].second;
    }
    if (!ok) {
        ++failures;
        std::printf("FAIL  %s\n", name);
    }
}

static void expect_probe(const char *name, vera_checker &checker, const std::string &piece, bool finalize,
                         bool expected) {
    const bool got = checker.would_introduce_diagnostic(piece, finalize);
    if (got != expected) {
        ++failures;
        std::printf("FAIL  %s\n", name);
    }
}

int main() {
    using expected = std::pair<std::string, std::string>;

    expect("clean classes", "class A { x: int; } class B { y: A; }", {});
    expect("duplicate class", "class A {} class A {}", {expected{"A", "module"}});
    expect("duplicate field", "class Point { x: int; x: int; }", {expected{"x", "class"}});
    expect("same field in different classes", "class A { x: int; } class B { x: int; }", {});
    expect("field may shadow a module name", "class A {} class B { A: int; }", {});
    expect("forward type reference", "class A { b: B; } class B {}", {expected{"B", "class"}});
    // The type reference is rejected per character, so the diagnostic names the
    // prefix at the point it can no longer become a declared name.
    expect("self type reference", "class Node { next: Node; }", {expected{"N", "class"}});
    expect("type chain", "class A { n: int; } class B { a: A; } class C { b: B; }", {});

    vera_checker checker;
    checker.feed("class Foo {} class Foo");
    expect_probe("pending duplicate is not rejected", checker, "", false, false);
    expect_probe("identifier extension remains legal", checker, "X", false, false);
    expect_probe("closing body completes the duplicate", checker, " {}", false, true);
    expect_probe("eog rejects an incomplete class", checker, "", true, true);
    if (!checker.diagnostics().empty()) {
        ++failures;
        std::printf("FAIL  probes mutated checker\n");
    }

    checker.feed("X {} class Bar {}");
    checker.finalize();
    if (!checker.diagnostics().empty()) {
        ++failures;
        std::printf("FAIL  legal extension produced diagnostic\n");
    }

    if (failures != 0) {
        std::printf("%d test(s) failed\n", failures);
        return 1;
    }
    std::printf("all tests passed\n");
    return 0;
}
