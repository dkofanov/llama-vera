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

using OptA = Optional<Char::Exact<'a'>>;
using AB = ExpandableRule<Char::Exact<'a'>, Char::Exact<'b'>>;
using OptThenB = ExpandableRule<Optional<Char::Exact<'a'>>, Char::Exact<'b'>>;
using OptThenA = ExpandableRule<Optional<AB>, Char::Exact<'a'>>;
using Choice = FirstMatch<ExpandableRule<Char::Exact<'a'>, Char::Exact<'c'>>,
                          ExpandableRule<Optional<Char::Exact<'a'>>, Char::Exact<'b'>>>;

} // namespace

int main() {
    // Optional as the root: present, absent, and pending.
    {
        Context context(64, std::in_place_type<OptA>);
        Expect(Feed(context, "a"), "root: present 'a'");
    }
    {
        Context context(64, std::in_place_type<OptA>);
        Expect(Feed(context, "x"), "root: absent 'x'");
    }
    {
        Context context(64, std::in_place_type<OptA>);
        Expect(Feed(context, ""), "root: empty feed is pending");
        Expect(Feed(context, "a"), "root: pending then present 'a'");
    }

    // Optional followed by a continuation.
    {
        Context context(64, std::in_place_type<OptThenB>);
        Expect(Feed(context, "b"), "seq: absent then 'b'");
    }
    {
        Context context(64, std::in_place_type<OptThenB>);
        Expect(Feed(context, "ab"), "seq: present 'a' then 'b'");
    }
    {
        Context context(64, std::in_place_type<OptThenB>);
        Expect(Feed(context, "a"), "seq: present 'a' pending");
        Expect(Feed(context, "b"), "seq: present 'a' then 'b' across feeds");
    }
    {
        Context context(64, std::in_place_type<OptThenB>);
        Expect(!Feed(context, "c"), "seq: absent then continuation fails on 'c'");
    }
    {
        Context context(64, std::in_place_type<OptThenB>);
        Expect(Feed(context, "a"), "seq: greedy commits to present 'a'");
        Expect(!Feed(context, "c"), "seq: no fallback to absent after commit");
    }

    // Backtracking into Optional across feeds: (ab)? 'a' on "a" then "c".
    {
        Context context(64, std::in_place_type<OptThenA>);
        Expect(Feed(context, "a"), "backtrack: partial 'a' is pending");
        Expect(Feed(context, "c"), "backtrack: 'ab' fails, 'a' matches continuation");
    }

    // Optional nested inside a choice alternative.
    {
        Context context(64, std::in_place_type<Choice>);
        Expect(Feed(context, "ab"), "choice: second alt uses Optional");
    }
    {
        Context context(64, std::in_place_type<Choice>);
        Expect(Feed(context, "ac"), "choice: first alt");
    }
    {
        Context context(64, std::in_place_type<Choice>);
        Expect(!Feed(context, "ax"), "choice: rejects 'ax'");
    }

    if (failures == 0) {
        std::printf("all optional tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", failures);
    return 1;
}
