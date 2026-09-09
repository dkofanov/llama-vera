#include "vera_analyzer.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

static int failures = 0;

static void expect(const char * name, const std::string & source,
                   const std::vector<std::pair<std::string, std::string>> & expected) {
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

static void expect_probe(const char * name, vera_checker & checker, const std::string & piece,
                         bool finalize, bool expected) {
    const bool got = checker.would_introduce_diagnostic(piece, finalize);
    if (got != expected) {
        ++failures;
        std::printf("FAIL  %s\n", name);
    }
}

int main() {
    using expected = std::pair<std::string, std::string>;

    expect("clean program",
           "function main(): null { let x: int = 1; let y: int = 2; return null; }",
           {});
    expect("duplicate function",
           "function foo(): null {} function foo(): null {}",
           {expected{"foo", "module"}});
    expect("duplicate parameter",
           "function foo(a: int, a: int): int { return a; }",
           {expected{"a", "function"}});
    expect("duplicate field", "class Point { x: int; x: int; }", {expected{"x", "class"}});
    expect("duplicate local",
           "function foo(): null { let x: int = 1; let x: int = 2; }",
           {expected{"x", "block"}});
    expect("allowed shadowing", "function foo(foo: int): int { return foo; }", {});

    vera_checker checker;
    checker.feed("function foo(): null {} function foo");
    expect_probe("pending duplicate is not rejected", checker, "", false, false);
    expect_probe("identifier extension remains legal", checker, "X", false, false);
    expect_probe("delimiter closes duplicate", checker, "(", false, true);
    expect_probe("eog closes duplicate", checker, "", true, true);
    if (!checker.diagnostics().empty()) {
        ++failures;
        std::printf("FAIL  probes mutated checker\n");
    }

    checker.feed("X(): null {}");
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
