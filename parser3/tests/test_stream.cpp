// Feeds a two-byte token across separate Feed calls: the grammar is ambiguous
// until the second byte ('<' then '=' or '>'), so this checks that a partial
// parse resumes when more input arrives.

#include "parser3/core/context.h"
#include "parser3/core/context_view.h"
#include "parser3/core/grammar_element.h"

#include <cstdio>
#include <utility>

using namespace vera::parser3;

namespace {

using Alt0 = ExpandableRule<Char::Exact<'<'>, Char::Exact<'='>>;
using Alt1 = ExpandableRule<Char::Exact<'<'>, Char::Exact<'>'>>;
using Root = FirstMatch<Alt0, Alt1>;

} // namespace

int main() {
    Context ctx(64, std::in_place_type<Root>);
    ContextView view(ctx);

    ctx.chars.Append("<", 1);
    if (!ctx.stack.Head()->Feed(view)) {
        std::printf("FAIL  stream: pending on '<'\n");
        return 1;
    }
    ctx.chars.Append(">", 1);
    if (!ctx.stack.Head()->Feed(view)) {
        std::printf("FAIL  stream: accepts '<>'\n");
        return 1;
    }

    std::printf("stream test passed\n");
    return 0;
}
