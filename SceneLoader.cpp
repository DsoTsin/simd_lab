#include "SceneLoader.h"
#include <fstream>
#include <iostream>
#include <chrono>  

using namespace std::chrono;


namespace simd {
Object::Object() : mesh_(nullptr) {}
Object::~Object() {}
void Object::set(SMesh *mesh, const FMatrix &l2w) {
  mesh_ = mesh;
  l2w_ = l2w;
}
SceneLoader::SceneLoader() {}
SceneLoader::~SceneLoader() {}
bool SceneLoader::load(const char *fileName) {
  std::ifstream input(fileName, std::ios::binary | std::ios::in);
  if (!input.good())
    return false;
  input.read((char *)&view_, sizeof(view_));
  input.read((char *)&viewProj_, sizeof(viewProj_));
  uint32 numObjects = 0;
  input.read((char *)&numObjects, sizeof(numObjects));
  check(numObjects > 0 && numObjects < 1000);
  objects_.reserve(numObjects);

  for (uint32 i = 0; i < numObjects; i++) {
    std::vector<FVector> positions;
    uint32 numTris = 0;
    input.read((char *)&numTris, sizeof(numTris));
    positions.resize(numTris * 3);
    input.read((char *)positions.data(), sizeof(FVector) * numTris * 3);

    auto newMesh = sr_.newMesh(positions.data(), numTris * 3);
#if 0
    FVector v[3];
    newMesh->getTriangle(v, numTris - 1);
    FVector lv[3];
    lv[0] = positions[(numTris - 1) * 3];
    lv[1] = positions[(numTris - 1) * 3 + 1];
    lv[2] = positions[(numTris - 1) * 3 + 2];
#endif

    FMatrix local2World;
    input.read((char *)local2World.M, sizeof(local2World));
    auto obj = new Object;
    obj->set(newMesh, local2World);
    objects_.push_back(obj);
  }

  return true;
}
void SceneLoader::allocFramebuffer(int width, int height) {
  sr_.init(width, height);
}

void SceneLoader::renderFrame() {
  //set breakpoint at SceneView.h:402, copy debug values
  FMatrix viewMatrix = {
      -0.939014971, -0.164611682, -0.301917553, 0.00000000,
     -0.343876719, 0.449500710, 0.824438155, 0.00000000,
     0.00000000, 0.877981782, -0.478693873, 0.00000000,
     -859.312927, 806.839966, 5204.81934, 1.00000000
  };
  FMatrix projMatrix = {
      0.999f, 0.f, 0.f, 0.f, 0.f, 1.998f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 10.f, 0.f,
  };
  //WClip = ViewProj.M[3][2];
  FMatrix viewProj;
  matrixMultiplyAVX(viewProj, viewMatrix, projMatrix);
  sr_.beginRender(viewProj);
  auto start = system_clock::now();
  for (auto &obj : objects_) {
    sr_.render(obj->mesh(), obj->l2w());
  }
  sr_.flush();

  auto end = system_clock::now();
  auto duration = duration_cast<microseconds>(end - start);

  sr_.endRender();

  std::cout << "using "
       << double(duration.count()) * microseconds::period::num /
              microseconds::period::den
       << " s" << std::endl;
  sr_.dumpDepthBuffer();
}
} // namespace simd