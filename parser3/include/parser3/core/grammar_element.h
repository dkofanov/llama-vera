#pragma once

#include "parser3/core/context_view.h"
#include "parser3/core/grammar_stack.h"
#include "parser3/util/constexpr_bitset.h"
#include "parser3/util/type_traits.h"

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

namespace vera::parser3 {

template <class Derived, std::size_t AdditionalSlots = 0>
struct SafepointFrame : GrammarFrame<Derived, AdditionalSlots, RewindTarget> {
    SafepointFrame() {
        static_assert(SafepointFrame::RequiredSlots <= 255, "Safepoint frame too large for uint8_t");
        this->slotSize_ = static_cast<std::uint8_t>(SafepointFrame::RequiredSlots);
        this->catch_ = [](RewindTarget &grammar, ContextView ctx) {
            return static_cast<Derived &>(grammar).Catch(ctx);
        };
    }

  protected:
    void SaveState(ContextView ctx) {
        RewindTarget *self = static_cast<RewindTarget *>(this);
        self->savedCurrent_ = ctx.chars.CurrentOffset();
        self->savedSemanticWatermark_ = ctx.stack.Semantics() != nullptr ? ctx.stack.Semantics()->Watermark() : 0;
        self->previousRewindTarget_.Set(self, ctx.stack.CurrentRewindTarget());
        ctx.stack.SetRewindTarget(self);
    }

    void Close(ContextView ctx) {
        RewindTarget *self = static_cast<RewindTarget *>(this);
        ctx.stack.SetRewindTarget(self->previousRewindTarget_.Get(self));
    }

    // Establish the rewind checkpoint on the first Feed; on later Feeds (a
    // resume) do nothing. Returns true on the first Feed.
    bool Begin(ContextView ctx) {
        RewindTarget *self = static_cast<RewindTarget *>(this);
        if (ctx.stack.CurrentRewindTarget() == self) {
            return false;
        }
        SaveState(ctx);
        return true;
    }

    // Close the checkpoint and resume the continuation.
    bool Dissolve(ContextView ctx) {
        Close(ctx);
        return ctx.stack.Reduce(this, ctx);
    }
};

template <class... Elements>
struct ExpandableRule : GrammarFrame<ExpandableRule<Elements...>> {
    static_assert(sizeof...(Elements) > 0, "ExpandableRule cannot have an empty expansion");

    static constexpr bool Nullable = (Elements::Nullable && ...);

    using Expansion = std::tuple<Elements...>;

    bool Feed(ContextView ctx) {
        ctx.stack.Pop(this);
        PushExpansion<Expansion>(ctx.stack);
        using HeadElement = std::tuple_element_t<0, typename HasExpansion<ExpandableRule>::FlattenedFirst>;
        return static_cast<HeadElement *>(ctx.stack.Head())->Feed(ctx);
    }

  private:
    template <class Element, class = void>
    struct HasExpansion : std::false_type {
        using FlattenedFirst = std::tuple<Element>;
    };

    template <class Element>
    struct HasExpansion<Element, std::void_t<typename Element::Expansion>> : std::true_type {
        using First = std::tuple_element_t<0, typename Element::Expansion>;
        using FlattenedFirst = typename HasExpansion<First>::FlattenedFirst;
    };

    template <class Element>
    static void PushElement(GrammarStack &stack) {
        if constexpr (HasExpansion<Element>::value) {
            PushExpansion<typename Element::Expansion>(stack);
        } else {
            stack.Push<Element>();
        }
    }

    template <class Expansion>
    static void PushExpansion(GrammarStack &stack) {
        ForEachTupleType<ReversedTupleT<Expansion>>(
            [&stack](auto element) { PushElement<typename decltype(element)::Type>(stack); });
    }
};

template <class Bits>
struct CharMatcher : GrammarFrame<CharMatcher<Bits>> {
    static constexpr bool Nullable = false;
    using Set = Bits;

    bool Feed(ContextView ctx) {
        if (ctx.chars.Empty()) {
            // At end of input a terminal cannot match: reject so the pending
            // alternative unwinds. Otherwise the fragment is merely pending.
            return ctx.chars.Finished() ? ctx.stack.Throw(ctx) : true;
        }
        if (!Bits::Test(static_cast<unsigned char>(ctx.chars.Front()))) {
            return ctx.stack.Throw(ctx);
        }
        ctx.chars.Advance();
        return ctx.stack.Reduce(this, ctx);
    }
};

struct Char {
    template <char C>
    using Exact = CharMatcher<ConstexprBitset<256>::ExactBit<static_cast<unsigned char>(C)>>;

    template <char Lo, char Hi>
    using Range =
        CharMatcher<ConstexprBitset<256>::RangeBits<static_cast<unsigned char>(Lo), static_cast<unsigned char>(Hi)>>;

    template <class... Matchers>
    using Union = CharMatcher<ConstexprBitset<256, (Matchers::Set::Word(0) | ...), (Matchers::Set::Word(1) | ...),
                                              (Matchers::Set::Word(2) | ...), (Matchers::Set::Word(3) | ...)>>;

    template <class Matcher>
    using Complement = CharMatcher<ConstexprBitset<256, ~Matcher::Set::Word(0), ~Matcher::Set::Word(1),
                                                   ~Matcher::Set::Word(2), ~Matcher::Set::Word(3)>>;
};

// A reference to a named rule. `Rule` is normally a forward-declared struct
// deriving from a grammar element; it is pushed as a single frame instead of
// being flattened, which breaks the compile-time recursion. The frame stays
// below the rule and resumes (reduces) once the rule completes, so recursion is
// tracked by the runtime stack. A rule ref must target a non-nullable rule.
template <class Rule>
struct RuleRef : GrammarFrame<RuleRef<Rule>, 1> {
    static constexpr bool Nullable = false;

    bool Feed(ContextView ctx) {
        if (started_) {
            return ctx.stack.Reduce(this, ctx);
        }
        started_ = true;
        ctx.stack.Push<Rule>();
        return ctx.stack.Head()->Feed(ctx);
    }

    bool started_ = false;
};

// Customization point for zero-width signal frames (the `@notation` elements).
// The primary template accepts; authors specialize it per notation tag. A
// handler may update semantic state and returns false to reject the feed. It
// must not throw (signals are hard rejections, not backtracking points).
template <class Tag>
struct SignalAction {
    static bool Feed(ContextView) {
        return true;
    }
};

// A zero-width semantic frame. It consumes nothing and always reduces, unless
// the author's handler rejects the input by returning false (which fails the
// feed outright, without unwinding the grammar stack).
template <class Tag>
struct Signal : GrammarFrame<Signal<Tag>> {
    static constexpr bool Nullable = true;

    bool Feed(ContextView ctx) {
        if (!SignalAction<Tag>::Feed(ctx)) {
            // A rejected signal is a failed match: rewind to the enclosing
            // checkpoint (restoring the semantic watermark) and let the grammar
            // try another alternative; the feed ultimately fails if none match.
            return ctx.stack.Throw(ctx);
        }
        return ctx.stack.Reduce(this, ctx);
    }
};

// End-of-input terminal. Pending until the stream is finished; rejects any
// remaining bytes; reduces once at end of input.
struct Eof : GrammarFrame<Eof> {
    static constexpr bool Nullable = false;

    bool Feed(ContextView ctx) {
        if (!ctx.chars.Empty()) {
            return ctx.stack.Throw(ctx);
        }
        if (!ctx.chars.Finished()) {
            return true;
        }
        return ctx.stack.Reduce(this, ctx);
    }
};

template <class... Alternatives>
struct FirstMatch : SafepointFrame<FirstMatch<Alternatives...>, 1> {
    static_assert(sizeof...(Alternatives) > 0, "FirstMatch cannot have no alternatives");
    static constexpr std::size_t Count = sizeof...(Alternatives);
    static constexpr bool Nullable = (Alternatives::Nullable || ...);

    bool Feed(ContextView ctx) {
        if (!this->Begin(ctx)) {
            // Resumed: an alternative reduced back.
            return this->Dissolve(ctx);
        }
        trialIndex_ = 0;
        ctx.stack.template Push<std::tuple_element_t<0, std::tuple<Alternatives...>>>();
        return ctx.stack.Head()->Feed(ctx);
    }

    bool Catch(ContextView ctx) {
        ++trialIndex_;
        if (trialIndex_ >= Count) {
            this->Close(ctx);
            return ctx.stack.Throw(ctx);
        }
        return TryAlternative(ctx, std::index_sequence_for<Alternatives...>{});
    }

  private:
    template <std::size_t Index, class Alternative>
    bool TryIndex(ContextView ctx) {
        if (trialIndex_ != Index) {
            return false;
        }
        ctx.stack.template Push<Alternative>();
        return ctx.stack.Head()->Feed(ctx);
    }

    template <std::size_t... Indices>
    bool TryAlternative(ContextView ctx, std::index_sequence<Indices...>) {
        bool ok = false;
        ((ok = ok || TryIndex<Indices, std::tuple_element_t<Indices, std::tuple<Alternatives...>>>(ctx)), ...);
        return ok;
    }

    std::size_t trialIndex_ = 0;
};

// PEG `Element?`: match `Element` if it is present, otherwise match nothing.
template <class Element>
struct Optional : SafepointFrame<Optional<Element>> {
    static constexpr bool Nullable = true;

    bool Feed(ContextView ctx) {
        if (!this->Begin(ctx)) {
            // Resumed: `Element` completed, so dissolve as present.
            return this->Dissolve(ctx);
        }
        ctx.stack.Push<Element>();
        return ctx.stack.Head()->Feed(ctx);
    }

    bool Catch(ContextView ctx) {
        // `Element` failed; rewind already happened, so dissolve as absent.
        return this->Dissolve(ctx);
    }
};

// PEG `Element*`: match `Element` zero or more times.
template <class Element>
struct StarArray : SafepointFrame<StarArray<Element>> {
    static constexpr bool Nullable = true;
    static_assert(!Element::Nullable,
                  "StarArray element must consume input on success (it may not match the empty string)");

    bool Feed(ContextView ctx) {
        if (!this->Begin(ctx)) {
            // Resumed: the previous iteration completed. The operand is
            // non-nullable (enforced above), so it must have consumed input.
            // Move the rewind mark forward too, so a failed iteration discards
            // its own reductions without discarding earlier iterations'.
            VERA_CHECK(ctx.chars.CurrentOffset() != this->savedCurrent_, "StarArray element matched the empty string");
            this->savedCurrent_ = ctx.chars.CurrentOffset();
            this->savedSemanticWatermark_ = ctx.stack.Semantics() != nullptr ? ctx.stack.Semantics()->Watermark() : 0;
        }
        ctx.stack.Push<Element>();
        return ctx.stack.Head()->Feed(ctx);
    }

    bool Catch(ContextView ctx) {
        // An iteration failed; the offset was rewound to the last attempt's start.
        return this->Dissolve(ctx);
    }
};

} // namespace vera::parser3
