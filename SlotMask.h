#pragma once

#include <stdint.h>

typedef uint32_t uint32;
typedef uint64_t uint64;

namespace ispc {
class FSlotMask {
public:
  FSlotMask();
  ~FSlotMask();

  void init(uint32_t slotCount);
  bool anySlotUsed() const;
  void markUsed(uint32_t index);
  void markUnused(uint32_t index);
  uint32_t maxUsedSlot() const;

  uint32_t length() const { return length_; }

  operator bool() const;
  bool operator==(const FSlotMask &sm) const;
  void operator|=(const FSlotMask &sm);
  void operator&=(const FSlotMask &sm);

private:
  uint32_t length_;
  uint32_t *mask_;
};
} // namespace ispc

namespace D3D12RHI {
class FSlotMask {
public:
  FSlotMask();
  explicit FSlotMask(int LowestVar);

  void ClearBits(bool bZero = false);
  bool IsEmpty() const;

  void MarkSlotUsed(uint32 SlotIndex);
  void MarkSlotUnused(uint32 SlotIndex);

  uint32 GetMaxSlotId() const;

  FSlotMask operator<<(uint32 ShiftCount) const;
  FSlotMask operator>>(uint32 ShiftCount) const;
  FSlotMask operator&(const FSlotMask &Other) const;
  FSlotMask operator|(const FSlotMask &Other) const;
  FSlotMask operator~() const;

  void operator|=(const FSlotMask &Other);
  void operator&=(const FSlotMask &Other);
  bool operator==(const FSlotMask &Other) const;
  bool operator!=(const FSlotMask &Other) const;

  operator bool() const { return !IsEmpty(); }

  static FSlotMask Zero;

  // ~((1<<SlotNeeded) -1)
  static FSlotMask FreeSlotMask(uint32 SlotNeeded);
  // (1<<SlotNeeded) - 1
  static FSlotMask UsedSlotMask(uint32 SlotNeeded);

private:
  // R &= ~(Other)
  void AndNot(const FSlotMask &Other);

  union {
    uint32_t V[4];
    uint64_t D[2];
  };
};

void testSlotMask();
} // namespace D3D12RHI
