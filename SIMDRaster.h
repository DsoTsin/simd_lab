#pragma once
#include "CoreMinimal.h"

#include <atomic>
#include <thread>

#include "Raster.isph"
#include "vectorlist.hpp"

#include "CpuFeature.h"
#include "Task.h"

#define ISPC_DELEGATE(name, ret, mainType, ...)                                \
  ret name(__VA_ARGS__);                                                       \
  static ret _##name(mainType, __VA_ARGS__)

namespace simd {
class SMesh {
public:
  SMesh();
  virtual ~SMesh();

  void Release();
  void AddRef();
  virtual int32 numTris() const = 0;
  virtual bool getTriangle(FVector v[3], int triId) const = 0;

private:
  std::atomic_int count_;
};

class SThreadContext {
public:
  using thread_ptr = std::shared_ptr<std::thread>;
  using thread_ptrs = std::vector<thread_ptr>;

  SThreadContext();

private:
  thread_ptr thread_;
  thread_ptrs xtransform_threads_;
};

struct SBoxInt {
  int32 minX = 0;
  int32 minY = 0;
  int32 maxX = 0;
  int32 maxY = 0;
};

class SRenderContext;
class SDepthBuffer;
class SDepthTile {
public:
  SDepthTile(SRenderContext *context);
  ~SDepthTile();

  void tryRaster();

private:
  float *accessDepth();

  SRenderContext *context_;
  // Tile AABB
  SBoxInt box_;
  float minDepth_;
  float maxDepth_;
  uint32 *mask_;
  std::vector<int32> triList_;
};

// use for accelerating triangle binning (intersection)
class SDepthTileTree {
public:
};

class SDepthBuffer {
public:
  SDepthBuffer(int32 numThreads, int32 simdLanes, int32 width, int32 height,
               bool use16Bits = false);
  ~SDepthBuffer();

  bool useHalf() const { return use16Bits_; }
  int32 width() const { return alignedWidth_; }
  int32 height() const { return alignedHeight_; }

  void clear(float depthValue);

  float load(int32 x, int32 y) const;
  void store(int32 x, int32 y, float depth);

  float *data() { return depth32_; }

private:
  int32 simdLanes_;
  int32 alignedWidth_;
  int32 alignedHeight_;

  bool use16Bits_;
  union {
    float *depth32_;
    int16 *depth16_;
  };

  union {
    SDepthBufferIspc32 ispc32_;
    SDepthBufferIspc16 ispc16_;
  };

  friend class SRenderContext;
};

class STransformCullTask : public ITask {
public:
  STransformCullTask(SRenderContext *context, SMesh *mesh, const FMatrix &l2c,
                     const SRenderParams &param, int32 sTriId, int32 eTriId,
                     TLinkVector<SScreenTriangle>::Vector *tris);

  virtual ~STransformCullTask();

  void run() override;
  ISPC_DELEGATE(receiveTris, void, STransformCullTask *, void *userData,
                int tid, SScreenTriangle *tri);

private:
  SRenderContext *context_;
  SRenderContextIspc ispc_;
  TRefCountPtr<SMesh> mesh_;
  SRenderParams params_;
  int32 startTris_;
  int32 endTris_;
  FMatrix localToClip_;
  TLinkVector<SScreenTriangle>::Vector *clipTris_;
};

class SRenderContext {
public:
  SRenderContext(SIMDArch preferArch);
  virtual ~SRenderContext();

  void initFramebuffer(int width, int height);
  void beginRender(const FMatrix &viewProj);
  void transformAndCull(SMesh *m, const FMatrix &l2w);
  void splitClipTris();
  // near clip plane kill
  void prepareTiles();
  void spawnTileRasterTasks();
  void endRender();
  int simdCount() const { return simdLanes_; }

  void dumpDepthBufferForDebug();

private:
  ISPC_DELEGATE(receiveTris, void, SRenderContext *, void *userData, int tid,
                SScreenTriangle *st);

  void myTransform(const float *inAoSTris, int trisOffset, int numTris,
                   const FMatrix *local2clip, SRenderParams *params,
                   SRenderContextIspc *ispc, void *userData);

  void scalarRasterize();
  void vectorRasterize();

  const SScreenTriangle &accessTri(size_t index) const {
    return surviveTris_[index];
  }

  bool forceSIMD_;
  bool useReverseZ_;
  int simdLanes_;
  int coreCount_;
  
  SIMDArch selectedArch_;
  SRenderContextIspc ispc_;

  FMatrix viewProj_;
  SDepthBuffer *depth_;

  TLinkVector<SScreenTriangle> ctris_;
  std::vector<SScreenTriangle> surviveTris_;
  std::vector<TaskThread *> transWorkers_;
  std::vector<STransformCullTask *> tasks_;
  int32 workerCounter_;
  int64 processedTris_;

  friend class SRender;
  friend class SDepthTile;
  friend class STransformCullTask;
};

class SRender {
public:
  SRender();
  virtual ~SRender();

  void init(int32 width, int32 height, bool halfDepth = false);
  int32 width_aligned() const;
  int32 height_aligned() const;

  SMesh *newMesh(const FVector *positions, int32 numPositions);
  SMesh *newMesh(const FVector *positions, int32 numPositions,
                 const uint16 *indices, int32 numIndices);

  void beginRender(const FMatrix &viewProj);
  void render(SMesh *m, const FMatrix &l2w);

  void flush();
  void endRender();

  void dumpDepthBuffer();

private:
  SRenderContext renderContext_;
};

void TestRaster();
} // namespace simd