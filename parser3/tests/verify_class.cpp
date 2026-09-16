// Feed VERA programs to the class-constrained grammar and print a verdict:
//   ACCEPT  - a complete, valid program
//   PREFIX  - a valid prefix (nothing committed violated a constraint yet)
//   REJECT  - a constraint (or syntax) violation
// Usage: parser3-verify-class FILE...
//
// Used to prove that generated programs are valid (never REJECT), and that
// crafted programs referencing a not-yet-declared class are REJECTed.

#include "parser3/core/context.h"
#include "parser3/core/context_view.h"
#include "parser3/examples/full_sem_grammar.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

using namespace vera::parser3;
using vera::parser3::full_sem::Root;

static const char *Check(const std::string &text) {
    Context ctx(1 << 14, std::in_place_type<Root>, std::make_unique<ClassSemantics>());
    ctx.chars.Append(text.data(), text.size());
    ContextView view(ctx);
    if (!ctx.stack.Head()->Feed(view)) {
        return "REJECT";
    }
    if (ctx.stack.Head() == nullptr) {
        return "ACCEPT";
    }
    ctx.chars.Finish();
    if (ctx.stack.Head()->Feed(view) && ctx.stack.Head() == nullptr) {
        return "ACCEPT";
    }
    return "PREFIX";
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s FILE...\n", argv[0]);
        return 2;
    }
    for (int i = 1; i < argc; i++) {
        std::ifstream in(argv[i]);
        std::stringstream buffer;
        buffer << in.rdbuf();
        std::printf("%-40s %s\n", argv[i], Check(buffer.str()));
    }
    return 0;
}
