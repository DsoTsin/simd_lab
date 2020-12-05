#pragma once

namespace simd {
enum class SIMDArch { SSE2, SSE4, AVX2, AVX512 };
class CpuFeature {
public:
  static CpuFeature &g();
  int maxSIMDWidth() const;
  SIMDArch maxSIMDArch() const;
private:
  CpuFeature();
  ~CpuFeature();
  int width_;
  SIMDArch arch_;
};
} // namespace simd