#include "Mesh_p.h"
#include "SIMDRaster.h"

namespace simd {

// mesh in aosoa form
SMesh::SMesh() : count_(1) {}
SMesh::~SMesh() {}

void SMesh::Release() { --count_; }

void SMesh::AddRef() { ++count_; }

SwizzledMesh::SwizzledMesh(int simdLanes, const FVector *positions, int32 numTris) : tris_(simdLanes){
  tris_.init(positions, numTris);
}

SwizzledMesh::SwizzledMesh(int simdLanes, const FVector *positions, int32 numPositions,
                           const uint16 *indices, int32 numIndices) : tris_(simdLanes) {
  tris_.init(positions, indices, numIndices / 3);
}

bool SwizzledMesh::getTriangle(FVector v[3], int triId) const {
  return tris_.get(v, triId);
}

const float *SwizzledMesh::data() const { return tris_.data(); }

int32 SwizzledMesh::numTris() const { return tris_.num(); }

} // namespace simd