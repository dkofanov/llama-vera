#pragma once

#include "parser3/ltoken/ltoken.h"

#include <cstddef>

namespace vera::parser3 {

class LTokenRegion {
  public:
    explicit LTokenRegion(std::size_t capacity) : allocator_(capacity * AlignedLToken::RequiredSlots) {}

    template <LTokenKind Kind>
    LToken<Kind> *Append(const LToken<Kind> &token) {
        static_assert(sizeof(LToken<Kind>) == sizeof(AlignedLToken));
        static_assert(alignof(LToken<Kind>) == alignof(AlignedLToken));
        return allocator_.Emplace<LToken<Kind>>(token);
    }

  private:
    LTokenAllocator allocator_;
};

} // namespace vera::parser3
