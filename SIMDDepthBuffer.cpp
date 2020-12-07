#include "SIMDRaster.h"

namespace simd {
bool SScreenTriangle::intersect(const SBoxInt &box) const {
  return (minY <= box.maxY && minY >= box.minY) ||
         (maxY <= box.maxY && maxY >= box.minY) || (maxY > box.maxY && minY < box.minY);
}

SDepthTile::SDepthTile(SRenderContext *context)
    : context_(context), minDepth_(0.f), maxDepth_(0.f), mask_(nullptr) {}

SDepthTile::~SDepthTile() {
  context_ = nullptr;
  if (mask_) {
    // free
    mask_ = nullptr;
  }
}

/*
 * two methods to rasterize
 * 1. barycentric rasterization
 * 2. edge-triangle scanline
 */
void SDepthTile::tryRaster() {
  // sort triList first

  for (size_t triId = 0; triId < triList_.size(); triId++) {
    auto &st = context_->accessTri(triList_[triId]); // cache problem ?
  }
}

float *SDepthTile::accessDepth() { return context_->depth_->data(); }

SDepthBuffer::SDepthBuffer(int32 numThreads, int32 simdLanes, int32 width,
                           int32 height, bool use16Bits)
    : simdLanes_(simdLanes),
      alignedWidth_((width & ~(simdLanes_ - 1)) + simdLanes_),
      alignedHeight_((height & ~(simdLanes_ - 1)) + simdLanes_),
      use16Bits_(use16Bits), depth32_(nullptr) {
  check(width > 0 && height > 0);
  check(alignedWidth_ > 0 && alignedHeight_ > 0);
  if (use16Bits_) {
    depth16_ = (int16 *)_aligned_malloc(
        sizeof(int16) * alignedWidth_ * alignedHeight_, simdLanes_ * 4);
    ispc16_.width = alignedWidth_;
    ispc16_.height = alignedHeight_;
    ispc16_.depth = depth16_;
  } else {
    depth32_ = (float *)_aligned_malloc(
        sizeof(float) * alignedWidth_ * alignedHeight_, simdLanes_ * 4);
    ispc32_.width = alignedWidth_;
    ispc32_.height = alignedHeight_;
    ispc32_.depth = depth32_;
  }
}
SDepthBuffer::~SDepthBuffer() {
  if (depth16_) {
    _aligned_free(depth16_);
    depth16_ = nullptr;
  }
  alignedWidth_ = 0;
  alignedHeight_ = 0;
}

extern "C" void clearDepth32(uniform float depth[], uniform float value,
                             uniform int32 width, uniform int32 height);
extern "C" void clearDepth16(uniform int16 depth[], uniform float value,
                             uniform int32 width, uniform int32 height);
void SDepthBuffer::clear(float depthValue) {
  if (use16Bits_) {
    clearDepth16(depth16_, depthValue, alignedWidth_, alignedHeight_);
  } else {
    clearDepth32(depth32_, depthValue, alignedWidth_, alignedHeight_);
  }
}
float SDepthBuffer::load(int32 x, int32 y) const {
  check(x < alignedWidth_ && y < alignedHeight_);
  if (use16Bits_) {
    return depth16_[y * alignedWidth_ + x]; // todo
  } else {
    return depth32_[y * alignedWidth_ + x];
  }
}
void SDepthBuffer::store(int32 x, int32 y, float depth) {
  check(x < alignedWidth_ && y < alignedHeight_);
  if (use16Bits_) {
    depth16_[y * alignedWidth_ + x] = depth; // todo
  } else {
    depth32_[y * alignedWidth_ + x] = depth;
  }
}
} // namespace simd