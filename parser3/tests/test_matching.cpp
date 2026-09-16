#include "parser3/core/context.h"
#include "parser3/core/context_view.h"
#include "parser3/core/grammar_element.h"

#include <cstdio>
#include <cstring>
#include <utility>

using namespace vera::parser3;

namespace {

int failures = 0;

void Expect(bool condition, const char *name) {
    if (condition) {
        std::printf("PASS  %s\n", name);
    } else {
        ++failures;
        std::printf("FAIL  %s\n", name);
    }
}

bool Feed(Context &context, const char *bytes) {
    context.chars.Append(bytes, std::strlen(bytes));
    return context.stack.Head()->Feed(ContextView{context});
}

// "<=" or "<>" share the '<' prefix, exercising backtracking across feeds.
using SharedAlt0 = ExpandableRule<Char::Exact<'<'>, Char::Exact<'='>>;
using SharedAlt1 = ExpandableRule<Char::Exact<'<'>, Char::Exact<'>'>>;
using SharedRoot = FirstMatch<SharedAlt0, SharedAlt1>;
using Digit = Char::Range<'0', '9'>;

} // namespace

int main() {
    {
        Context context(64, std::in_place_type<SharedRoot>);
        Expect(Feed(context, "<"), "shared prefix: pending on '<'");
        Expect(Feed(context, ">"), "shared prefix: backtracks to '<>'");
    }
    {
        Context context(64, std::in_place_type<SharedRoot>);
        Expect(Feed(context, "<="), "shared prefix: '<=' in one feed");
    }
    {
        Context context(64, std::in_place_type<SharedRoot>);
        Expect(!Feed(context, "x"), "shared prefix: rejects 'x'");
    }
    {
        Context context(64, std::in_place_type<Digit>);
        Expect(Feed(context, "7"), "Char::Range accepts an in-range byte");
    }
    {
        Context context(64, std::in_place_type<Digit>);
        Expect(!Feed(context, "x"), "Char::Range rejects an out-of-range byte");
    }
    {
        Context context(64, std::in_place_type<Char::Exact<'a'>>);
        Expect(!Feed(context, "b"), "Char::Exact rejects a different byte");
    }

    if (failures == 0) {
        std::printf("all matching tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", failures);
    return 1;
}
