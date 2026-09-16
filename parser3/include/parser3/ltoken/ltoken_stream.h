#pragma once

#include "parser3/core/context.h"
#include "parser3/core/context_view.h"
#include "parser3/ltoken/ltoken_region.h"

#include <cstddef>
#include <utility>

namespace vera::parser3 {

template <class Root>
class LTokenStream {
  public:
    LTokenStream(std::size_t tokenCapacity, std::size_t stackCapacity)
        : region_(tokenCapacity), context_(stackCapacity, std::in_place_type<Root>) {}

    ContextView View() {
        return ContextView{context_};
    }

  private:
    LTokenRegion region_;
    Context context_;
    LToken<LTokenKind::Frontier> head_;
};

} // namespace vera::parser3
