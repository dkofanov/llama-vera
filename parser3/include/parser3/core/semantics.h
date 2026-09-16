#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace vera::parser3 {

// A byte range in the CharStream buffer.
struct Span {
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
};

// Author-owned semantic state attached to a Context. It is cloned on Fork,
// adopted on Commit, and rewound to a watermark whenever the grammar
// backtracks. A watermark is an opaque checkpoint the state can capture (e.g.
// its undo-log length) and restore.
class SemanticState {
  public:
    virtual ~SemanticState() = default;
    virtual std::unique_ptr<SemanticState> Clone() const = 0;
    virtual void CopyFrom(const SemanticState &other) = 0;
    virtual std::size_t Watermark() const = 0;
    virtual void Rewind(std::size_t watermark) = 0;
};

} // namespace vera::parser3
