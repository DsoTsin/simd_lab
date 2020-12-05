// simd_intersector.cpp : This file contains the 'main' function. Program
// execution begins and ends there.
//
#include <iostream>
#include <vector>

#include "CoreMinimal.h"

#include "SIMDRaster.h"
#include "SlotMask.h"
#include "assert.h"

extern bool intersect(float *origin, float *extent, float *permutated_plane);

int main() {
  // rasterize Epic SunTemple scene
  simd::TestRaster();
  D3D12RHI::testSlotMask();

  return 0;
}
