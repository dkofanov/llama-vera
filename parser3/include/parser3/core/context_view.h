#pragma once

namespace vera::parser3 {

class CharStream;
class GrammarStack;
struct Context;

struct ContextView {
    CharStream &chars;
    GrammarStack &stack;

    // Defined in context.h, once Context is complete.
    explicit ContextView(Context &context);
};

} // namespace vera::parser3
