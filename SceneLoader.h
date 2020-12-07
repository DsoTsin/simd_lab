#pragma once
#include "CoreMinimal.h"
#include <chrono>  
#include "SIMDRaster.h"

namespace simd {
class SMesh;

class Object {
public:
  Object();
  ~Object();
  void set(SMesh *mesh, const FMatrix &l2w);
  SMesh *mesh() { return mesh_; }
  const FMatrix &l2w() { return l2w_; }
private:
  SMesh *mesh_;
  FMatrix l2w_;
};

class SceneLoader {
public:
  SceneLoader();
  ~SceneLoader();
  bool load(const char *fileName);

  void allocFramebuffer(int width, int height);
  void renderFrame();

  void dump();

private:
  FMatrix view_;
  FMatrix viewProj_;

  TArray<Object *> objects_;
  SRender sr_;

  std::chrono::microseconds duration;
};
} // namespace simd