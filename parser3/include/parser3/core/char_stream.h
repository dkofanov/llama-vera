#pragma once

#include "parser3/core/input_store.h"
#include "parser3/util/check.h"

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace vera::parser3 {

// The input view of one Context.  Committed bytes live in a shared InputStore;
// bytes fed since the last commit live in this context's private suffix.  Reads
// fall through: [0, prefixLen) comes from the store, the rest from the suffix.
class CharStream {
  public:
    CharStream() : store_(std::make_shared<InputStore>()) {}

    void Append(const char *data, std::size_t size) {
        if (size == 0) {
            return;
        }
        suffix_.insert(suffix_.end(), data, data + size);
        end_ += size;
    }

    // Marks the end of input: an empty stream is then a final "no more bytes"
    // rather than a pending fragment, so terminals reject instead of waiting.
    void Finish() {
        finished_ = true;
    }

    bool Finished() const {
        return finished_;
    }

    std::size_t Size() const {
        return end_;
    }

    bool Empty() const {
        return current_ == end_;
    }

    char Front() const {
        VERA_CHECK(!Empty(), "CharStream has no remaining input");
        return At(current_);
    }

    void Advance() {
        VERA_CHECK(!Empty(), "CharStream cannot advance past its input");
        ++current_;
    }

    std::size_t CurrentOffset() const {
        return current_;
    }

    // Byte at an absolute offset, regardless of the committed/suffix split.
    char ByteAt(std::size_t index) const {
        VERA_CHECK(index < end_, "CharStream byte index out of range");
        return At(index);
    }

    void SetCurrentOffset(std::size_t offset) {
        VERA_CHECK(offset <= end_, "CharStream offset out of range");
        current_ = offset;
    }

    std::string_view Slice(std::size_t begin, std::size_t end) const {
        VERA_CHECK(begin <= end && end <= end_, "CharStream slice out of range");
        if (end <= prefixLen_) {
            return std::string_view(store_->Data() + begin, end - begin);
        }
        if (begin >= prefixLen_) {
            return std::string_view(suffix_.data() + (begin - prefixLen_), end - begin);
        }
        VERA_CHECK(false, "CharStream slice crosses the committed boundary");
        return {};
    }

    // A fork shares the committed store and copies the (bounded) speculative
    // suffix, so forking does not copy the committed input.
    CharStream Fork() const {
        return CharStream(*this);
    }

    // Adopt `other`'s view without copying the committed bytes (the store is
    // shared). Used to reset a reusable scratch context to a committed state.
    void AssignFrom(const CharStream &other) {
        store_ = other.store_;
        prefixLen_ = other.prefixLen_;
        suffix_ = other.suffix_;
        current_ = other.current_;
        end_ = other.end_;
        finished_ = other.finished_;
    }

    // An independent copy: the committed store is duplicated rather than shared.
    // Two such copies can each be committed without disturbing the other, which
    // is what `Fork` cannot guarantee.
    CharStream DeepCopy() const {
        CharStream out;
        out.store_ = std::make_shared<InputStore>(store_->DeepCopy());
        out.prefixLen_ = prefixLen_;
        out.suffix_ = suffix_;
        out.current_ = current_;
        out.end_ = end_;
        out.finished_ = finished_;
        return out;
    }

    // Move a fork's suffix into the shared store and take over its position.
    void CommitFrom(CharStream &&fork) {
        store_->Append(fork.suffix_.data(), fork.suffix_.size());
        prefixLen_ = store_->Size();
        suffix_.clear();
        end_ = prefixLen_;
        current_ = fork.current_;
        finished_ = fork.finished_;
    }

  private:
    char At(std::size_t index) const {
        return index < prefixLen_ ? store_->Data()[index] : suffix_[index - prefixLen_];
    }

    std::shared_ptr<InputStore> store_;
    std::size_t prefixLen_ = 0;
    std::vector<char> suffix_;
    std::size_t current_ = 0;
    std::size_t end_ = 0;
    bool finished_ = false;
};

} // namespace vera::parser3
