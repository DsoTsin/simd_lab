#pragma once

#include "SIMDRaster.h"
#include "SoATriangles.h"

namespace simd {
class SwizzledMesh final : public SMesh {
private:
  SoATriangles tris_;
  int simdLanes_;
public:
  SwizzledMesh(int simdLanes, const FVector *positions, int32 numTris);
  SwizzledMesh(int simdLanes, const FVector *positions, int32 numPositions,
               const uint16 *indices, int32 numIndices);
  bool getTriangle(FVector v[3], int triId) const override;

  const float *data() const;
  int32 numTris() const override;
};

} // namespace simd