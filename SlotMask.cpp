#include "SlotMask.h"
#include "SlotMaskOps.ispc.h"

#include <assert.h>

namespace ispc {

FSlotMask::FSlotMask() : length_(0), mask_(nullptr) {}

FSlotMask::~FSlotMask() {}

void FSlotMask::init(uint32_t slotCount) { assert(slotCount % 64 == 0); }

bool FSlotMask::anySlotUsed() const { return false; }

void FSlotMask::markUsed(uint32_t index) {}

void FSlotMask::markUnused(uint32_t index) {}

uint32_t FSlotMask::maxUsedSlot() const { return uint32_t(); }

FSlotMask::operator bool() const { return false; }

bool FSlotMask::operator==(const FSlotMask &sm) const {
  return length_ == sm.length_ &&
         ispc::TestAllBitEqual(length_, mask_, sm.mask_);
}

void FSlotMask::operator|=(const FSlotMask &sm) {}

void FSlotMask::operator&=(const FSlotMask &sm) {}

} // namespace ispc

#include <immintrin.h> //avx2
#include <intrin.h>

namespace D3D12RHI {
FSlotMask::FSlotMask() { _mm_storeu_si128((__m128i *)V, _mm_setzero_si128()); }

FSlotMask::FSlotMask(int LowestVar) {
  _mm_storeu_si128((__m128i *)V, _mm_setzero_si128());
  V[0] = LowestVar;
}

// ~((1<<SlotNeeded) -1)
FSlotMask FSlotMask::FreeSlotMask(uint32 SlotNeeded) {
  FSlotMask Mask;
  Mask.ClearBits(false);
  return Mask << SlotNeeded;
}

// (1<<SlotNeeded) - 1
FSlotMask FSlotMask::UsedSlotMask(uint32 SlotNeeded) {
  FSlotMask Mask;
  if (SlotNeeded < 64) {
    Mask.D[0] = (1ull << SlotNeeded) - 1ull;
  } else {
    Mask.D[0] = 0xffffffffffffffff;
    Mask.D[1] = (1ull << (SlotNeeded - 64)) - 1ull;
  }
  return Mask;
}

void FSlotMask::ClearBits(bool bZero) {
  if (bZero) {
    _mm_storeu_si128((__m128i *)V, _mm_setzero_si128());
  } else {
    D[0] = 0xffffffffffffffff;
    D[1] = 0xffffffffffffffff;
  }
}

bool FSlotMask::IsEmpty() const { return (D[0] == 0) && (D[1] == 0); }

static FSlotMask Lowest1 = FSlotMask(1);

// |= (1 << SlotIndex)
void FSlotMask::MarkSlotUsed(uint32 SlotIndex) {
  this->operator|=(Lowest1 << SlotIndex);
}

// &= ~(1 << SlotIndex)
void FSlotMask::MarkSlotUnused(uint32 SlotIndex) {
  auto Used = Lowest1 << SlotIndex;
  AndNot(Used);
}

uint32 FSlotMask::GetMaxSlotId() const {
  auto high = (uint32)__lzcnt64(D[1]);
  if (high == 64) {
    return 64 - (uint32)__lzcnt64(D[0]);
  } else // high == 0
  {
    return 128 - high;
  }
}

FSlotMask FSlotMask::operator<<(uint32 ShiftCount) const {
  __m128i v = _mm_loadu_si128((__m128i const *)V);
  __m128i v1, v2;
  if ((ShiftCount) >= 64) {
    v1 = _mm_slli_si128(v, 8);
    v1 = _mm_slli_epi64(v1, (ShiftCount)-64);
  } else {
    v1 = _mm_slli_epi64(v, ShiftCount);
    v2 = _mm_slli_si128(v, 8);
    v2 = _mm_srli_epi64(v2, 64 - (ShiftCount));
    v1 = _mm_or_si128(v1, v2);
  }
  FSlotMask Ret;
  _mm_storeu_si128((__m128i *)Ret.V, v1);
  return Ret;
}

FSlotMask FSlotMask::operator>>(uint32 ShiftCount) const {
  __m128i v = _mm_loadu_si128((__m128i const *)V);
  __m128i v1, v2;
  if ((ShiftCount) >= 64) {
    v1 = _mm_srli_si128(v, 8);
    v1 = _mm_srli_epi64(v1, (ShiftCount)-64);
  } else {
    v1 = _mm_srli_epi64(v, ShiftCount);
    v2 = _mm_srli_si128(v, 8);
    v2 = _mm_slli_epi64(v2, 64 - (ShiftCount));
    v1 = _mm_or_si128(v1, v2);
  }
  FSlotMask Ret;
  _mm_storeu_si128((__m128i *)Ret.V, v1);
  return Ret;
}

FSlotMask FSlotMask::operator&(const FSlotMask &Other) const {
  __m128i v0 = _mm_loadu_si128((__m128i const *)V);
  __m128i v1 = _mm_loadu_si128((__m128i const *)Other.V);
  __m128i v2 = _mm_and_si128(v0, v1);
  FSlotMask Ret;
  _mm_storeu_si128((__m128i *)Ret.V, v2);
  return Ret;
}

FSlotMask FSlotMask::operator|(const FSlotMask &Other) const {
  __m128i v0 = _mm_loadu_si128((__m128i const *)V);
  __m128i v1 = _mm_loadu_si128((__m128i const *)Other.V);
  __m128i v2 = _mm_or_si128(v0, v1);
  FSlotMask Ret;
  _mm_storeu_si128((__m128i *)Ret.V, v2);
  return Ret;
}

FSlotMask FSlotMask::operator~() const {
  // Returns ~x, the bitwise complement of x:
  __m128i v0 = _mm_loadu_si128((__m128i const *)V);
  __m128i v1 = _mm_xor_si128(
      v0, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
  FSlotMask Ret;
  _mm_storeu_si128((__m128i *)Ret.V, v1);
  return Ret;
}

void FSlotMask::operator|=(const FSlotMask &Other) {
  __m128i v0 = _mm_loadu_si128((__m128i const *)V);
  __m128i v1 = _mm_loadu_si128((__m128i const *)Other.V);
  __m128i v2 = _mm_or_si128(v0, v1);
  _mm_storeu_si128((__m128i *)V, v2);
}

void FSlotMask::operator&=(const FSlotMask &Other) {
  __m128i v0 = _mm_loadu_si128((__m128i const *)V);
  __m128i v1 = _mm_loadu_si128((__m128i const *)Other.V);
  __m128i v2 = _mm_and_si128(v0, v1);
  _mm_storeu_si128((__m128i *)V, v2);
}

bool FSlotMask::operator==(const FSlotMask &Other) const {
  __m128i v0 = _mm_loadu_si128((__m128i const *)V);
  __m128i v1 = _mm_loadu_si128((__m128i const *)Other.V);
  __m128i vcmp = _mm_cmpeq_epi32(v0, v1);
  uint16_t mask = _mm_movemask_epi8(vcmp);
  return mask == 0xffff;
}

bool FSlotMask::operator!=(const FSlotMask &Other) const {
  return !operator==(Other);
}

void FSlotMask::AndNot(const FSlotMask &Other) {
  __m128i v0 = _mm_loadu_si128((__m128i const *)V);
  __m128i v1 = _mm_loadu_si128((__m128i const *)Other.V);
  __m128i v2 = _mm_andnot_si128(v1, v0);
  _mm_storeu_si128((__m128i *)V, v2);
}

FSlotMask FSlotMask::Zero;

void testSlotMask() {

  ispc::FSlotMask sm0;
  sm0.init(256);

  ispc::FSlotMask sm1;
  sm1.init(256);

  D3D12RHI::FSlotMask M = D3D12RHI::FSlotMask::Zero;
  M.MarkSlotUsed(1);
  M.MarkSlotUsed(0);
  M.MarkSlotUsed(3);
  auto MaxSlotId = M.GetMaxSlotId();
  assert(MaxSlotId == 4);
  // bool bEquals = sm0 == sm1;
  M.MarkSlotUnused(3);
  MaxSlotId = M.GetMaxSlotId();
  assert(MaxSlotId == 2);

  M.MarkSlotUsed(111);
  MaxSlotId = M.GetMaxSlotId();
  assert(MaxSlotId == 112);

  M.MarkSlotUsed(72);

  M.MarkSlotUnused(111);
  MaxSlotId = M.GetMaxSlotId();
  assert(MaxSlotId == 73);

  /*FConvexVolume Volume;
  FBoxSphereBounds B = { {1.0,3.5,2.3}, {1.0,-3.f,2.f}, 3.f};
  Volume.IntersectBox(B.Origin, B.BoxExtent);
  intersect((float*)&B.Origin, (float*)&B.BoxExtent,
  (float*)Volume.PermutedPlanes.data());*/
}
} // namespace D3D12RHI