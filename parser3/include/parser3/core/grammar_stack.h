#pragma once

#include "parser3/core/char_stream.h"
#include "parser3/core/context_view.h"
#include "parser3/core/grammar_frame.h"
#include "parser3/core/semantics.h"

#include <cstddef>
#include <utility>

namespace vera::parser3 {

class GrammarStack {
  public:
    template <class Root>
    GrammarStack(std::size_t capacity, std::in_place_type_t<Root>) : allocator_(capacity) {
        current_ = allocator_.Emplace<Root>();
        current_->previous_.Set(current_, nullptr);
    }

    // Deep copy the used frames into a fresh allocator and relocate the absolute
    // pointers (intra-stack references are slot-relative offsets, so they survive
    // the contiguous copy). Semantics are not carried over yet.
    GrammarStack Clone() const {
        GrammarStack out(CloneTag{}, allocator_.Capacity());
        out.allocator_.CopyStorageFrom(allocator_, allocator_.Used());
        const std::byte *oldBase = allocator_.Data();
        std::byte *newBase = out.allocator_.Data();
        out.current_ = Relocate(current_, oldBase, newBase);
        out.rewindTarget_ = Relocate(rewindTarget_, oldBase, newBase);
        out.semantics_ = nullptr;
        return out;
    }

    // Copy `other`'s used frames into this (already-allocated) storage and
    // relocate the absolute pointers. Unlike Clone this does not allocate, so it
    // is the cheap reset for a reusable trial context. Callers must ensure this
    // stack's capacity covers `other`'s used slots.
    void AssignFrom(const GrammarStack &other) {
        const std::size_t used = other.allocator_.Used();
        VERA_CHECK(used <= allocator_.Capacity(), "GrammarStack assignment exceeds capacity");
        allocator_.CopyStorageFrom(other.allocator_, used);
        const std::byte *oldBase = other.allocator_.Data();
        std::byte *newBase = allocator_.Data();
        current_ = Relocate(other.current_, oldBase, newBase);
        rewindTarget_ = Relocate(other.rewindTarget_, oldBase, newBase);
        // semantics_ is owned by this stack's Context and points at its own
        // state object; only the alternative source's frames are copied here,
        // so leave the pointer untouched.
    }

    ExpectedGrammar *Head() {
        return current_;
    }

    const ExpectedGrammar *Head() const {
        return current_;
    }

    template <class Element>
    Element *Push() {
        Element *element = allocator_.Emplace<Element>();
        element->previous_.Set(element, current_);
        current_ = element;
        return element;
    }

    template <class Element>
    void Pop(Element *element) {
        VERA_CHECK(current_ == element, "GrammarStack can only pop its current element");
        current_ = element->previous_.Get(element);
        allocator_.Pop(element);
    }

    template <class Element>
    bool Reduce(Element *element, ContextView ctx) {
        VERA_CHECK(current_ == element, "GrammarStack can only reduce its current element");
        current_ = element->previous_.Get(element);
        allocator_.Pop(element);
        if (current_ != nullptr) {
            return current_->Feed(ctx);
        }
        return true;
    }

    // Optional author-owned semantic state; null means the grammar is purely
    // syntactic and no rewind/watermark work is done.
    void SetSemantics(SemanticState *semantics) {
        semantics_ = semantics;
    }

    SemanticState *Semantics() const {
        return semantics_;
    }

    template <class State>
    State *SemanticsAs() const {
        return static_cast<State *>(semantics_);
    }

    RewindTarget *CurrentRewindTarget() const {
        return rewindTarget_;
    }

    void SetRewindTarget(RewindTarget *target) {
        rewindTarget_ = target;
    }

    bool Throw(ContextView ctx) {
        if (rewindTarget_ == nullptr) {
            return false;
        }
        allocator_.Rewind(rewindTarget_, rewindTarget_->slotSize_);
        current_ = rewindTarget_;
        ctx.chars.SetCurrentOffset(rewindTarget_->savedCurrent_);
        if (semantics_ != nullptr) {
            semantics_->Rewind(rewindTarget_->savedSemanticWatermark_);
        }
        return rewindTarget_->Catch(ctx);
    }

  private:
    struct CloneTag {};

    GrammarStack(CloneTag, std::size_t capacity) : allocator_(capacity) {}

    template <class T>
    static T *Relocate(T *pointer, const std::byte *oldBase, std::byte *newBase) {
        if (pointer == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<T *>(newBase + (reinterpret_cast<const std::byte *>(pointer) - oldBase));
    }

    GrammarStackAllocator allocator_;
    ExpectedGrammar *current_;
    RewindTarget *rewindTarget_ = nullptr;
    SemanticState *semantics_ = nullptr;
};

} // namespace vera::parser3
