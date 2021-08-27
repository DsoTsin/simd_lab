#pragma once
#include "CoreMinimal.h"

namespace simd {
/*
triangle { FVector v0, v1, v2 } 9 floats
AoSoA { FVector v0 { x[ ] }, v1, v2 } 9 x simdLanes floats
{
    v0x0,v0x1,v0x2,v0x3
    v0y0,v0y1,v0y2,v0y3
    v0z0,v0z1,v0z2,v0z3

    v1x0,v1x1,v1x2,v1x3
    v1y0,v1y1,v1y2,v1y3
    v1z0,v1z1,v1z2,v1z3

    v2x0,v2x1,v2x2,v2x3
    v2y0,v2y1,v2y2,v2y3
    v2z0,v2z1,v2z2,v2z3
} 4 triangles paralleled
*/
struct SoATriangles {
  SoATriangles(int simdLanes);
  ~SoATriangles();

  void reserve(int trisCount);

  void init(const FVector *positions, int trisCount);
  void init(const FVector *positions, const uint16 *indices, int trisCount);
  bool get(FVector v[3], int triId) const;
  const float *data() const { return trisData_; }
  int32 num() const { return num_; }

private:
  int32 num_;
  int32 alignedNumTris_;
  int32 simdLanes_;
  float *trisData_;
};
} // namespace simd