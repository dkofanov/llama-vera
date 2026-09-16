#pragma once

#include "parser3/core/context_view.h"
#include "parser3/util/bump_allocator.h"

#include <cstddef>
#include <cstdint>

namespace vera::parser3 {

using GrammarStackAllocator = BumpAllocator<3>;

struct ExpectedGrammar {
    using FeedFunction = bool (*)(ExpectedGrammar &, ContextView);

    bool Feed(ContextView ctx) {
        return feed_(*this, ctx);
    }

    FeedFunction feed_;
    GrammarStackAllocator::RelativePointer<ExpectedGrammar> previous_;
};

struct RewindTarget : ExpectedGrammar {
    using CatchFunction = bool (*)(RewindTarget &, ContextView);

    bool Catch(ContextView ctx) {
        return catch_(*this, ctx);
    }

    std::uint8_t slotSize_ = 0;
    GrammarStackAllocator::RelativePointer<RewindTarget> previousRewindTarget_;
    CatchFunction catch_ = nullptr;
    std::size_t savedCurrent_ = 0;
    std::size_t savedSemanticWatermark_ = 0;
};

template <class T>
inline constexpr std::size_t
    FrameSlots = (sizeof(T) + GrammarStackAllocator::SlotSize - 1) / GrammarStackAllocator::SlotSize;

template <class Derived, std::size_t AdditionalSlots = 0, class Base = ExpectedGrammar>
struct GrammarFrame : Base, GrammarStackAllocator::RequiresSlots<FrameSlots<Base> + AdditionalSlots> {
    GrammarFrame() {
        this->feed_ = [](ExpectedGrammar &grammar, ContextView ctx) {
            return static_cast<Derived &>(grammar).Feed(ctx);
        };
    }
};

} // namespace vera::parser3
