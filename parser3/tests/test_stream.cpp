#include "parser3/core/context_view.h"
#include "parser3/core/grammar_element.h"
#include "parser3/ltoken/ltoken_stream.h"

#include <cstdio>

using namespace vera::parser3;

namespace {

using Alt0 = ExpandableRule<Char::Exact<'<'>, Char::Exact<'='>>;
using Alt1 = ExpandableRule<Char::Exact<'<'>, Char::Exact<'>'>>;
using Root = FirstMatch<Alt0, Alt1>;

} // namespace

int main() {
    LTokenStream<Root> stream(64, 64);
    ContextView view = stream.View();

    view.chars.Append("<", 1);
    if (!view.stack.Head()->Feed(view)) {
        std::printf("FAIL  stream: pending on '<'\n");
        return 1;
    }
    view.chars.Append(">", 1);
    if (!view.stack.Head()->Feed(view)) {
        std::printf("FAIL  stream: accepts '<>'\n");
        return 1;
    }

    std::printf("stream test passed\n");
    return 0;
}
