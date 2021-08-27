#define __AVX__
#include "SoftRender.h"
#include "Mesh_p.h"

#define DUMP_PNG 1

#ifdef DUMP_PNG
#include "lodepng.h"
#endif

#include "ScopeTimer.h"
#include <intrin.h>

#ifndef USE_MULTITHREAD
#define USE_MULTITHREAD 1
#endif
#ifndef USE_AVX
#define USE_AVX 1
#endif

#ifndef USE_WAIT
#define USE_WAIT 0
#endif

#ifndef USE_SCALAR_RASTERIZE
#define USE_SCALAR_RASTERIZE 0
#endif

#ifndef USE_SCALAR_BINNING
#define USE_SCALAR_BINNING 0
#endif

#ifndef USE_SINGLETHREAD_RASTER
#define USE_SINGLETHREAD_RASTER 0
#endif

// todo: consider using masked tiles with tile min/maxDepth to reduce triangle
// rasterization count
// @see masked occlusion culling by intel
// this library is designed to be compatible with mobile devices (ARM neon)

extern "C" int ispc_lane_width();

extern "C" void transformVerticesAndCullTriangles(const float *inAoSTris,
                                                  int trisOffset, int numTris,
                                                  const FMatrix *local2clip,
                                                  SRenderParams *params,
                                                  SRenderContextIspc *ispc,
                                                  void *userData);

extern "C" void transformVerticesAndCullTriangles_avx512skx(
    const float *inAoSTris, int trisOffset, int numTris,
    const FMatrix *local2clip, SRenderParams *params, SRenderContextIspc *ispc,
    void *userData);

extern "C" void transformVerticesAndCullTriangles_avx2(
    const float *inAoSTris, int trisOffset, int numTris,
    const FMatrix *local2clip, SRenderParams *params, SRenderContextIspc *ispc,
    void *userData);

extern "C" void transformVerticesAndCullTriangles_sse4(
    const float *inAoSTris, int trisOffset, int numTris,
    const FMatrix *local2clip, SRenderParams *params, SRenderContextIspc *ispc,
    void *userData);

extern "C" void transformVerticesAndCullTriangles_sse2(
    const float *inAoSTris, int trisOffset, int numTris,
    const FMatrix *local2clip, SRenderParams *params, SRenderContextIspc *ispc,
    void *userData);
export void rasterTriangles32F(simd::SScreenTriangle *tris, int32 numTris,
                               uniform uint8 useReverseZ,
                               uniform SDepthBufferIspc32 *uniform depth);

export void rasterTriangles32FImplReverseZTiled(
    uniform simd::SScreenTriangle tris[], uniform int32 trisIds[],
    uniform int32 numTris, uniform simd::SBoxInt *uniform tileBox,
    uniform SDepthBufferIspc32 *uniform depth);

export void
rasterTriangles32FImplReverseZTiled2(uniform simd::SScreenTriangle tris[],
                                     uniform int32 numTris,
                                     uniform simd::SBoxInt *uniform tileBox,
                                     uniform SDepthBufferIspc32 *uniform depth);

namespace simd {

STransformCullTask::STransformCullTask(
    SRenderContext *context, SMesh *mesh, const FMatrix &l2c,
    const SRenderParams &params, int32 sTriId, int32 eTriId,
    TLinkVector<SScreenTriangle>::Vector *tris)
    : context_(context), mesh_(mesh), localToClip_(l2c), params_(params),
      startTris_(sTriId), endTris_(eTriId), clipTris_(tris) {
  ispc_.ctx = (struct ::SRenderContext *)this;
  ispc_.recv = (FnReceiveTris)&STransformCullTask::_receiveTris;
}

STransformCullTask::~STransformCullTask() {}

void STransformCullTask::receiveTris(void *userData, int tid,
                                     SScreenTriangle *st) {
  TLinkVector<SScreenTriangle>::Vector *v =
      (TLinkVector<SScreenTriangle>::Vector *)userData;
  if (st->flag & ClipNear) {
    // need triangulate
  } else {
    bool shouldDiscard = false;
    v->add(*st);
  }
}
void STransformCullTask::_receiveTris(STransformCullTask *task, void *userData,
                                      int tid, SScreenTriangle *st) {
  task->receiveTris(userData, tid, st);
}
void STransformCullTask::run() {
  SwizzledMesh *sm = static_cast<SwizzledMesh *>(mesh_.GetReference());
  if (startTris_ == 0) {
    if (endTris_ < 2000) {
      clipTris_->reserve(endTris_);
    } else {
      clipTris_->reserve(endTris_ / 2);
    }
  }
  context_->transformAndCull(sm->data(), startTris_, endTris_, &localToClip_,
                        &params_, &ispc_, clipTris_);
}

void SRender::init(int32 width, int32 height, bool halfDepth) {
  renderContext_.initFramebuffer(width, height);
}
int32 SRender::width_aligned() const { return renderContext_.depth_->width(); }
int32 SRender::height_aligned() const {
  return renderContext_.depth_->height();
}
SRender::SRender() : renderContext_(SIMDArch::AVX512) {}
SRender::~SRender() {}
SMesh *SRender::newMesh(const FVector *positions, int32 numPositions) {
  if (!positions || numPositions == 0)
    return nullptr;
  return new SwizzledMesh(renderContext_.simdCount(), positions,
                          numPositions / 3);
}
SMesh *SRender::newMesh(const FVector *positions, int32 numPositions,
                        const uint16 *indices, int32 numIndices) {
  if (!positions || numPositions == 0 || !indices || numIndices == 0)
    return nullptr;
  return new SwizzledMesh(renderContext_.simdCount(), positions, numPositions,
                          indices, numIndices);
}

void SRender::beginRender(const FMatrix &viewProj) {
  renderContext_.beginRender(viewProj);
}
void SRender::render(const FVector* positions, int32 numPositions, const FMatrix& l2w)
{
    //TODO
}
void SRender::render(const FVector* positions, int32 numPositions, const uint16* indices, int32 numIndices, const FMatrix& l2w)
{
    //TODO
}
void SRender::render(SMesh *m, const FMatrix &l2w) {
  renderContext_.transformAndCull(m, l2w);
}

void SRender::flush() {
  renderContext_.splitClipTris();
  renderContext_.prepareTiles();
  renderContext_.spawnTileRasterTasks();
}
void SRender::endRender() { renderContext_.endRender(); }

void SRender::dumpDepthBuffer() { renderContext_.dumpDepthBufferForDebug(); }

SRenderContext::SRenderContext(SIMDArch preferArch)
    : forceSIMD_(true), useReverseZ_(true), simdLanes_(0), coreCount_(0),
      selectedArch_(preferArch), depth_(nullptr), workerCounter_(0),
      processedTris_(0) {
  SIMDArch maxArch = CpuFeature::g().maxSIMDArch();
  if (preferArch <= maxArch) {
    selectedArch_ = preferArch;
  } else {
    selectedArch_ = maxArch;
  }

  switch (selectedArch_) {
  case SIMDArch::AVX512:
    simdLanes_ = 16;
    break;
  case SIMDArch::AVX2:
    simdLanes_ = 8;
    break;
  case SIMDArch::SSE4:
  case SIMDArch::SSE2:
    simdLanes_ = 4;
    break;
  }

  ispc_.ctx = (struct ::SRenderContext *)this;
  ispc_.recv = (FnReceiveTris)&SRenderContext::_receiveTris;
  coreCount_ = GetCpuCoreNum();
  // consider big-little cpu?
  for (int i = 0; i < coreCount_; i++) {
    auto thread = new TaskThread;
    transWorkers_.Add(thread);
  }

  for (auto &w : transWorkers_) {
    w->launch();
  }
}
SRenderContext::~SRenderContext() {
  for (auto &w : transWorkers_) {
    delete w;
  }
}
void SRenderContext::receiveTris(void *userData, int tid, SScreenTriangle *st) {
  TLinkVector<SScreenTriangle>::Vector *v =
      (TLinkVector<SScreenTriangle>::Vector *)userData;
  v->add(*st);
}
void SRenderContext::_receiveTris(SRenderContext *ctx, void *userData, int tid,
                                  SScreenTriangle *st) {
  ctx->receiveTris(userData, tid, st);
}
void SRenderContext::initFramebuffer(int width, int height) {
  if (depth_) {
    delete depth_;
  }
  depth_ = new SDepthBuffer(coreCount_, simdLanes_, width, height);
}
void SRenderContext::beginRender(const FMatrix &viewProj) {
  check(depth_ != nullptr);
  processedTris_ = 0;
  viewProj_ = viewProj;
  // if use reverseZ, 0 means far, 1 means near
  useReverseZ_ = viewProj_.M[2][2] == 0.0f;
  // clear depth
  // rev CF_GreaterEqual : CF_LessEqual
  // depth_->clear(useReverseZ_ ? -INFINITY : INFINITY);
  depth_->clear(useReverseZ_ ? 0.0f : 1.0f);

  // clear tiles data
  tileBoxes_.Empty();
  tileTrisIds_.Empty();
  tileTris_.Empty();

  int32 laneCountX = depth_->width() / simdLanes_;
  int32 laneCountY = depth_->height() / simdLanes_;

  // for cache-miss issues, we prefer column-based task dispatch
  if (laneCountY > coreCount_) {
    int32 lanesYPerCore = laneCountY / coreCount_;
    int32 lanesYLastCore = laneCountY % coreCount_;

    int32 rowsPerCore = depth_->height() / coreCount_;
    int32 rowsLastCore = depth_->height() % coreCount_;

    int32 tileW = laneCountX * simdLanes_;
    int32 tileHPerCore = lanesYPerCore * simdLanes_;
    int32 tileHLastCore = lanesYLastCore * simdLanes_;

    for (int32 coreId = 0; coreId < coreCount_; coreId++) {
      if (coreId == coreCount_ - 1) {
        SBoxInt box{0, rowsPerCore * coreId, tileW,
                    rowsPerCore * coreId + tileHLastCore};
        tileBoxes_.Add(box);
        break;
      }
      SBoxInt box{0, rowsPerCore * coreId, tileW,
                  rowsPerCore * coreId + rowsPerCore};
      tileBoxes_.Add(box);
    }
  }

  // allocate tile trisIds
  tileTrisIds_.Resize(tileBoxes_.Num());
  tileTris_.Resize(tileBoxes_.Num());
}
void SRenderContext::transformAndCull(SMesh *m, const FMatrix &l2w) {
  processedTris_ += m->numTris();
  FMatrix l2c;
  matrixMultiplyAVX(l2c, l2w, viewProj_);
  SRenderParams params{depth_->width(), depth_->height(), viewProj_.M[3][2]};
  auto trisN = ctris_.newVector();
#if USE_MULTITHREAD
  transform_tasks_.Add(
      new STransformCullTask(this, m, l2c, params, 0, m->numTris(), trisN));
  workerCounter_ = ++workerCounter_ % transWorkers_.Num();
  transWorkers_[workerCounter_]->enqueue(transform_tasks_.Last());
#else
  SwizzledMesh *sm = static_cast<SwizzledMesh *>(m);
  trisN->reserve(sm->numTris());
  myTransform(sm->data(), 0, sm->numTris(), &l2c, &params, &ispc_, trisN);
#endif
  // split clip tris
}

extern "C" void
binTriangles2TileImpl(uniform SScreenTriangle tris[], uniform int32 numTris,
                      uniform SBoxInt *uniform tileBox, uniform int32 numTiles,
                      uniform FnBinTriangle2Tiles fnBinTriangle2Tiles,
                      SRenderContext *uniform self);

void SRenderContext::splitClipTris() {
  ScopeTimer _("splitClipTris");
#if USE_WAIT
  {
    ScopeTimer wt("trasnforms");
    for (auto &t : transform_tasks_) {
      t->wait();
    }
  }

  {
    ScopeTimer gt("gatherClipTris");
    // gather tris, memcpy heavy
    // todo: here, we already know visibility result for some mesh.
    surviveTris_.resize(ctris_.accum());
    size_t offset = 0;
    for (auto iter = ctris_.begin(); iter; iter++) {
      auto &vec = *iter;
      if (vec.num) {
        memcpy(&surviveTris_[offset], vec.data,
               vec.num * sizeof(SScreenTriangle));
        offset += vec.num;
      }
    }
  }
#else
  size_t total_tasks = transform_tasks_.Num();
  size_t task_id = 0;
  while (total_tasks > 0) {
    if (transform_tasks_[task_id]->isComplete()) {
      auto &tris = transform_tasks_[task_id]->clipTris();
      binTriangles2TileImpl(
          tris.data, tris.num, tileBoxes_.GetData(), int32(tileBoxes_.Num()),
          (FnBinTriangle2Tiles)&SRenderContext::_binTriangle2Tiles, this);
      total_tasks--;
    }
    task_id = ++task_id % transform_tasks_.Num();
  }
#endif
}

void SRenderContext::_binTriangle2Tiles(SRenderContext *self,
                                        const SScreenTriangle *tri,
                                        int32 tileId) {
  self->binTriangle2Tiles(tri, tileId);
}

void SRenderContext::binTriangle2Tiles(const SScreenTriangle *tri,
                                       int32 tileId) {
  if (tileTris_[tileId].Num() == 0) {
    tileTris_[tileId].Reserve(4000);
  }
  tileTris_[tileId].Add(*tri);
}

export void binTriangles(uniform SScreenTriangle tris[], uniform int32 numTris,
                         uniform SBoxInt *uniform tileBox,
                         uniform int32 numTiles, uniform FnBinTiles fnBinTiles,
                         SRenderContext *uniform self);

// assign tris to tiles
// foreach triangle intersects tiles, tiles add tris ids (tile quad tree?)
// (if totally covered in a tile, the best performance gains)
// classify big triangles (immediate rasterize) and small triangles (go tiled)
void SRenderContext::prepareTiles() {
  ScopeTimer _("prepareTiles");
  // assign tris to tiles
  check(depth_ != nullptr);

#if !USE_SINGLETHREAD_RASTER && USE_WAIT
#if USE_SCALAR_BINNING
  for (size_t index = 0; index < surviveTris_.size(); index++) {
    auto &st = surviveTris_[index];
    for (size_t tileId = 0; tileId < tileBoxes_.size(); tileId++) {
      if (st.intersect(tileBoxes_[tileId])) {
        tileTrisIds_[tileId].push_back((int32)index);
      }
    }
  }
#else
  binTriangles(surviveTris_.data(), int32(surviveTris_.size()),
               tileBoxes_.data(), int32(tileBoxes_.size()),
               (FnBinTiles)&SRenderContext::_binTiles, this);
#endif
  // todo sort triList?
#endif
}

void SRenderContext::_binTiles(SRenderContext *self, int32 triId,
                               int32 tileId) {
  self->binTiles(triId, tileId);
}

void SRenderContext::binTiles(int32 triId, int32 tileId) {
  if (tileTrisIds_[tileId].Num() == 0) {
    tileTrisIds_[tileId].Reserve(4000);
  }
  tileTrisIds_[tileId].Add(triId);
}

SRasterTask::SRasterTask(SRenderContext *context, int32 tileId)
    : context_(context), tileId_(tileId) {}

SRasterTask::~SRasterTask() {}

void SRasterTask::run() {
#if USE_WAIT
  rasterTriangles32FImplReverseZTiled(
      context_->surviveTris_.data(), context_->tileTrisIds_[tileId_].data(),
      (int32)context_->tileTrisIds_[tileId_].size(),
      &context_->tileBoxes_[tileId_], context_->accessDepth32());
#else
  rasterTriangles32FImplReverseZTiled2(
      context_->tileTris_[tileId_].GetData(),
      (int32)context_->tileTris_[tileId_].Num(),
      &context_->tileBoxes_[tileId_], context_->accessDepth32());
#endif
}

void SRenderContext::spawnTileRasterTasks() {
  ScopeTimer _("rasterize");
#if USE_SINGLETHREAD_RASTER
#if USE_SCALAR_RASTERIZE
  scalarRasterize();
#else
  vectorRasterize();
#endif
#else
  for (int32 tileId = 0; tileId < tileBoxes_.Num(); tileId++) {
    raster_tasks_.Add(new SRasterTask(this, tileId));
    transWorkers_[tileId]->enqueue(raster_tasks_.Last());
  }
  for (auto &r : raster_tasks_) {
    r->wait();
    delete r;
  }
  raster_tasks_.Empty();
#endif
}

void SRenderContext::scalarRasterize() {
  for (size_t index = 0; index < surviveTris_.Num(); index++) {
    auto &st = surviveTris_[index];
    float area = edgeFunction(st.sx0, st.sy0, st.sx1, st.sy1, st.sx2, st.sy2);
    for (int32 x = int32(st.minX); x <= int32(st.maxX); x++)
      for (int32 y = int32(st.minY); y <= int32(st.maxY); y++) {
        float pixelX = x + 0.5f;
        float pixelY = y + 0.5f;
        float w0 = edgeFunction(st.sx1, st.sy1, st.sx2, st.sy2, pixelX, pixelY);
        float w1 = edgeFunction(st.sx2, st.sy2, st.sx0, st.sy0, pixelX, pixelY);
        float w2 = edgeFunction(st.sx0, st.sy0, st.sx1, st.sy1, pixelX, pixelY);
        if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
          w0 /= area;
          w1 /= area;
          w2 /= area;
          float oneOverZ =
              1.0f / st.cz0 * w0 + 1.0f / st.cz0 * w1 + 1.0f / st.cz0 * w2;
          float z = 1.0f / oneOverZ; // need?
          if (useReverseZ_) {
            if (z > depth_->load(x, y)) {
              depth_->store(x, y, z);
            }
          } else {
            if (z < depth_->load(x, y)) {
              depth_->store(x, y, z);
            }
          } // reverse z condition
        }
      }
  }
}

void SRenderContext::vectorRasterize() {
  rasterTriangles32F(surviveTris_.GetData(), (int32)surviveTris_.Num(),
                     useReverseZ_, &depth_->ispc32_);
}

void SRenderContext::endRender() {
  ScopeTimer _("endRender");
  for (auto &t : transform_tasks_) {
    delete t;
  }
  transform_tasks_.Empty();
  surviveTris_.Empty();
  ctris_.reset();
}

extern "C" void exportDepthBufferRGBA(uniform float *depthValues,
                                      uniform int width, uniform int height,
                                      uniform uint8 useReverseZ,
                                      uniform uint8 *cachePixels);

void SRenderContext::dumpDepthBufferForDebug() {
  std::vector<uint8> pixels;
  pixels.resize(4 * depth_->width() * depth_->height());
  exportDepthBufferRGBA(depth_->data(), depth_->width(), depth_->height(),
                        useReverseZ_, pixels.data());
  std::vector<uint8> data;
  lodepng::encode(data, pixels.data(), depth_->width(), depth_->height(),
                  LCT_RGBA, 8);
  lodepng::save_file(data, "depth_buffer.png");
}

void SRenderContext::transformAndCull(const float *inAoSTris, int trisOffset,
                                 int numTris, const FMatrix *local2clip,
                                 SRenderParams *params,
                                 SRenderContextIspc *ispc, void *userData) {
  switch (selectedArch_) {
  case SIMDArch::SSE2:
    transformVerticesAndCullTriangles_sse2(inAoSTris, trisOffset, numTris,
                                           local2clip, params, ispc, userData);
    break;
  case SIMDArch::SSE4:
    transformVerticesAndCullTriangles_sse4(inAoSTris, trisOffset, numTris,
                                           local2clip, params, ispc, userData);
    break;
  case SIMDArch::AVX2:
    transformVerticesAndCullTriangles_avx2(inAoSTris, trisOffset, numTris,
                                           local2clip, params, ispc, userData);
    break;
  case SIMDArch::AVX512:
    transformVerticesAndCullTriangles_avx512skx(
        inAoSTris, trisOffset, numTris, local2clip, params, ispc, userData);
    break;
  }
}

SThreadContext::SThreadContext() {}

} // namespace simd