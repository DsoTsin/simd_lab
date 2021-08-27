#include "SoATriangles.h"
#include "SoftRender.h"

namespace simd {
struct __flat_triangle {
  union {
    FVector v[3];
    float f[9];
  };
};

SoATriangles::SoATriangles(int simdLanes)
    : num_(0), alignedNumTris_(0), simdLanes_(simdLanes), trisData_(0) {}
SoATriangles::~SoATriangles() {
  num_ = 0;
  alignedNumTris_ = 0;
  if (trisData_) {
    _aligned_free(trisData_);
    trisData_ = nullptr;
  }
}
void SoATriangles::reserve(int trisCount) {
  int alignedTrisCount = (trisCount & ~(simdLanes_ - 1)) + simdLanes_;
  uint32 totalBytes = 9 * sizeof(float) * alignedTrisCount;
  trisData_ = (float *)_aligned_malloc(totalBytes, sizeof(float) * simdLanes_);
  ::memset(trisData_, 0, totalBytes);
}
// need check scattering intrin support (avx 512 ? gathering in avx2)
void SoATriangles::init(const FVector *positions, int trisCount) {
  // one triangle consumes 9 floats
  int alignedTrisCount = (trisCount & ~(simdLanes_ - 1)) + simdLanes_;
  int numGangs = alignedTrisCount / simdLanes_;
  alignedNumTris_ = alignedTrisCount;
  num_ = trisCount;
  if (trisData_) {
    _aligned_free(trisData_);
  }
  uint32 totalBytes = 9 * sizeof(float) * alignedNumTris_;
  trisData_ = (float *)_aligned_malloc(totalBytes, sizeof(float) * simdLanes_);
  ::memset(trisData_, 0, totalBytes);
  // scatter store
  const __flat_triangle *ft = (const __flat_triangle *)positions;
  for (int gangId = 0; gangId < numGangs; gangId++) {
    float *curGangPtr = trisData_ + gangId * simdLanes_ * 9;
    const __flat_triangle *curGangTris = ft + gangId * simdLanes_;
    for (int laneId = 0; laneId < simdLanes_; laneId++) {
      int triId = gangId * simdLanes_ + laneId;
      if (triId < num_) {
        for (int fvId = 0; fvId < 9; fvId++) {
          curGangPtr[fvId * simdLanes_ + laneId] = curGangTris[laneId].f[fvId];
        }
      }
    }
  }
}
void SoATriangles::init(const FVector *positions, const uint16 *indices,
                        int trisCount) {
  // one triangle consumes 9 floats
  int alignedTrisCount = (trisCount & ~(simdLanes_ - 1)) + simdLanes_;
  alignedNumTris_ = alignedTrisCount;
  num_ = trisCount;
  if (trisData_) {
    _aligned_free(trisData_);
  }
  uint32 totalBytes = 9 * sizeof(float) * alignedNumTris_;
  trisData_ = (float *)_aligned_malloc(totalBytes, sizeof(float) * simdLanes_);
  ::memset(trisData_, 0, totalBytes);
  // todo
}
bool SoATriangles::get(FVector v[3], int triId) const {
  if (triId >= num_)
    return false;
  int gangId = triId / simdLanes_;
  int laneId = triId % simdLanes_;
  int floatsPerGang = 9 * simdLanes_;
  float *gangPtr = trisData_ + floatsPerGang * gangId;
  v[0].X = gangPtr[laneId];
  v[0].Y = gangPtr[laneId + simdLanes_];
  v[0].Z = gangPtr[laneId + 2 * simdLanes_];

  v[1].X = gangPtr[laneId + 3 * simdLanes_];
  v[1].Y = gangPtr[laneId + 4 * simdLanes_];
  v[1].Z = gangPtr[laneId + 5 * simdLanes_];

  v[2].X = gangPtr[laneId + 6 * simdLanes_];
  v[2].Y = gangPtr[laneId + 7 * simdLanes_];
  v[2].Z = gangPtr[laneId + 8 * simdLanes_];
  return true;
}
} // namespace simd