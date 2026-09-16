#pragma once

#include "parser3/ltoken/ltoken_registry.h"
#include "parser3/util/bump_allocator.h"

#include <cstddef>
#include <cstdint>
#include <tuple>

namespace vera::parser3 {

using LTokenAllocator = BumpAllocator<3>;

struct AlignedLToken : LTokenAllocator::RequiresSlots<3> {
    std::uint32_t sourceOffset_;
    LTokenKind kind_;
    LTokenStates state_;

    template <class State>
    const State &Extract() const {
        return std::get<State>(state_);
    }

    std::byte *Next() {
        return reinterpret_cast<std::byte *>(this) + sizeof(*this);
    }
};

template <LTokenKind Kind>
struct LToken : AlignedLToken {
    static constexpr LTokenKind StaticKind = Kind;

    LToken() {
        kind_ = Kind;
    }

    const StateOf<Kind> &Extract() const {
        return AlignedLToken::Extract<StateOf<Kind>>();
    }
};

} // namespace vera::parser3
