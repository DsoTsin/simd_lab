#pragma once
#include "CoreMinimal.h"
#include "SoftRender.isph"
#include "CpuFeature.h"
#include "Task.h"

#ifndef USE_STD_THREAD
#define USE_STD_THREAD 0
#endif

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
  using thread_ptrs = TArray<thread_ptr>;

  SThreadContext();

private:
  thread_ptr thread_;
  thread_ptrs xtransform_threads_;
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
  TArray<int32> triList_;
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
  TLinkVector<SScreenTriangle>::Vector &clipTris() { return *clipTris_; }

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

class SRasterTask : public ITask {
public:
  SRasterTask(SRenderContext *context, int32 tileId);
  virtual ~SRasterTask();

  void run() override;

private:
  SRenderContext *context_;
  int32 tileId_;
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
  ISPC_DELEGATE(binTiles, void, SRenderContext *context, int32 triId,
                int32 tileId);

  ISPC_DELEGATE(binTriangle2Tiles, void, SRenderContext* context, const SScreenTriangle* tri,
      int32 tileId);

  ISPC_DELEGATE(receiveTris, void, SRenderContext *, void *userData, int tid,
                SScreenTriangle *st);

  void transformAndCull(const float *inAoSTris, int trisOffset, int numTris,
                   const FMatrix *local2clip, SRenderParams *params,
                   SRenderContextIspc *ispc, void *userData);

  SDepthBufferIspc32 *accessDepth32() { return &depth_->ispc32_; }

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
  TArray<SBoxInt> tileBoxes_;
  TArray<TArray<int32>> tileTrisIds_;
  TArray<TArray<SScreenTriangle>> tileTris_;

  TLinkVector<SScreenTriangle> ctris_;
  TArray<SScreenTriangle> surviveTris_;
  TArray<TaskThread *> transWorkers_;
  TArray<STransformCullTask *> transform_tasks_;
  TArray<SRasterTask *> raster_tasks_;
  int32 workerCounter_;
  int64 processedTris_;

  friend class SRender;
  friend class SDepthTile;
  friend class STransformCullTask;
  friend class SRasterTask;
};

class SRender {
public:
  SRender();
  virtual ~SRender();

  void init(int32 width, int32 height, bool halfDepth = false);
  int32 width_aligned() const;
  int32 height_aligned() const;

  /// <summary>
  ///   Create SOA-form triangles mesh
  /// </summary>
  /// <param name="positions"></param>
  /// <param name="numPositions"></param>
  /// <returns></returns>
  SMesh *newMesh(const FVector *positions, int32 numPositions);

  /// <summary>
  ///   Create SOA-form triangles mesh
  /// </summary>
  /// <param name="positions"></param>
  /// <param name="numPositions"></param>
  /// <param name="indices"></param>
  /// <param name="numIndices"></param>
  /// <returns></returns>
  SMesh *newMesh(const FVector *positions, int32 numPositions,
                 const uint16 *indices, int32 numIndices);

  void beginRender(const FMatrix &viewProj);

  /// <summary>
  ///   Render triangles (without SOA-form)
  /// </summary>
  /// <param name="positions"></param>
  /// <param name="numPositions"></param>
  /// <param name="l2w"></param>
  void render(const FVector* positions, int32 numPositions, const FMatrix& l2w);

  /// <summary>
  ///   Render indexed triangles (without SOA-form)
  /// </summary>
  /// <param name="positions"></param>
  /// <param name="numPositions"></param>
  /// <param name="indices"></param>
  /// <param name="numIndices"></param>
  /// <param name="l2w"></param>
  void render(const FVector* positions, int32 numPositions,
      const uint16* indices, int32 numIndices, const FMatrix& l2w);

  /// <summary>
  ///   Render SOA-optimized mesh
  /// </summary>
  /// <param name="m"></param>
  /// <param name="l2w"></param>
  void render(SMesh *m, const FMatrix &l2w);

  void flush();
  void endRender();

  void dumpDepthBuffer();

private:
  SRenderContext renderContext_;
};

void TestRaster();
} // namespace simd