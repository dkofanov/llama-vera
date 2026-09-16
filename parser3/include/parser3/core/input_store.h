#pragma once

#include <cstddef>
#include <vector>

namespace vera::parser3 {

// The single shared, append-only store of committed input bytes.  Every Context
// in a parse refers to one instance; bytes are appended when a ptoken commits
// and are never modified afterwards, so contexts can share it and keep reading
// it through a prefix-length snapshot.
class InputStore {
  public:
    void Append(const char *data, std::size_t size) {
        if (size == 0) {
            return;
        }
        bytes_.insert(bytes_.end(), data, data + size);
    }

    std::size_t Size() const {
        return bytes_.size();
    }

    const char *Data() const {
        return bytes_.data();
    }

    // A private copy of the committed bytes. Unlike a shared store (which is what
    // Context::Fork adopts), the copy is independent, so a subsequently committed
    // clone cannot observe or corrupt it.
    InputStore DeepCopy() const {
        return *this;
    }

  private:
    std::vector<char> bytes_;
};

} // namespace vera::parser3
