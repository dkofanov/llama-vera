#pragma once

#include "parser3/util/check.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace vera::parser3 {

template <std::size_t N>
class BumpAllocator {
  public:
    static constexpr std::size_t SlotSize = std::size_t{1} << N;

    template <class T>
    class RelativePointer {
      public:
        T *Get(void *origin) const {
            if (slots_ == Null) {
                return nullptr;
            }
            return reinterpret_cast<T *>(reinterpret_cast<std::byte *>(origin) + slots_ * SlotSize);
        }

        const T *Get(const void *origin) const {
            if (slots_ == Null) {
                return nullptr;
            }
            return reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(origin) + slots_ * SlotSize);
        }

        void Set(const void *origin, T *target) {
            if (target == nullptr) {
                slots_ = Null;
                return;
            }
            const std::byte *from = reinterpret_cast<const std::byte *>(origin);
            const std::byte *to = reinterpret_cast<const std::byte *>(target);
            const std::ptrdiff_t bytes = to - from;
            VERA_CHECK(bytes % static_cast<std::ptrdiff_t>(SlotSize) == 0,
                       "RelativePointer target is not slot-aligned");
            const std::ptrdiff_t slots = bytes / static_cast<std::ptrdiff_t>(SlotSize);
            VERA_CHECK(slots >= std::numeric_limits<std::int16_t>::min() + 1 &&
                           slots <= std::numeric_limits<std::int16_t>::max(),
                       "RelativePointer target is out of range");
            slots_ = static_cast<std::int16_t>(slots);
        }

      private:
        static constexpr std::int16_t Null = std::numeric_limits<std::int16_t>::min();
        std::int16_t slots_ = Null;
    };

    template <std::size_t Slots>
    struct alignas(SlotSize) RequiresSlots {
        static_assert(Slots > 0);
        static constexpr std::size_t RequiredSlots = Slots;
    };

    explicit BumpAllocator(std::size_t slotCapacity)
        // Default-initialise: `make_unique<Slot[]>` would zero the whole capacity
        // (memset), which dominates a fork that only touches a few slots.
        : storage_(slotCapacity == 0 ? nullptr : std::unique_ptr<Slot[]>(new Slot[slotCapacity])),
          slotCapacity_(slotCapacity) {}

    template <class T, class... Args>
    T *Emplace(Args &&...args) {
        static_assert(std::is_base_of_v<RequiresSlots<T::RequiredSlots>, T>);
        static_assert(alignof(T) <= SlotSize);
        static_assert(sizeof(T) <= T::RequiredSlots * SlotSize);
        static_assert(std::is_trivially_destructible_v<T>);
        // Always-on: an exhaustion here would otherwise write out of bounds in
        // release builds (VERA_CHECK is compiled out under NDEBUG).
        if (T::RequiredSlots > slotCapacity_ - nextSlot_) {
            VERA_FAIL("BumpAllocator capacity exceeded");
        }
        T *value = ::new (static_cast<void *>(&storage_[nextSlot_])) T(std::forward<Args>(args)...);
        nextSlot_ += T::RequiredSlots;
        return value;
    }

    template <class T>
    void Pop(T *value) {
        static_assert(std::is_base_of_v<RequiresSlots<T::RequiredSlots>, T>);
        VERA_CHECK(nextSlot_ >= T::RequiredSlots, "BumpAllocator is empty");
        VERA_CHECK(value == reinterpret_cast<T *>(&storage_[nextSlot_ - T::RequiredSlots]),
                   "BumpAllocator can only pop its latest allocation");
        nextSlot_ -= T::RequiredSlots;
    }

    void Rewind(void *frame, std::size_t slots) {
        const std::byte *base = reinterpret_cast<const std::byte *>(storage_.get());
        const std::byte *from = reinterpret_cast<const std::byte *>(frame);
        const std::size_t offset = static_cast<std::size_t>(from - base) / SlotSize;
        VERA_CHECK(offset + slots <= slotCapacity_, "BumpAllocator rewind out of range");
        nextSlot_ = offset + slots;
    }

    std::byte *Data() {
        return reinterpret_cast<std::byte *>(storage_.get());
    }

    const std::byte *Data() const {
        return reinterpret_cast<const std::byte *>(storage_.get());
    }

    std::size_t Capacity() const {
        return slotCapacity_;
    }

    std::size_t Used() const {
        return nextSlot_;
    }

    // Copy `slots` used slots from `other` into this (empty) allocator. Slot
    // alignment is preserved, so RelativePointers between the copied frames stay
    // valid as long as the callers relocate their absolute pointers.
    void CopyStorageFrom(const BumpAllocator &other, std::size_t slots) {
        VERA_CHECK(slots <= slotCapacity_, "BumpAllocator copy exceeds capacity");
        VERA_CHECK(slots <= other.nextSlot_, "BumpAllocator copy source has too few slots");
        if (slots != 0) {
            std::memcpy(storage_.get(), other.storage_.get(), slots * SlotSize);
        }
        nextSlot_ = slots;
    }

  private:
    struct alignas(SlotSize) Slot {
        std::byte bytes_[SlotSize];
    };

    std::unique_ptr<Slot[]> storage_;
    std::size_t slotCapacity_;
    std::size_t nextSlot_ = 0;
};

} // namespace vera::parser3
