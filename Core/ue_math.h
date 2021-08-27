#pragma once

#include <stdint.h>

typedef int64_t int64;
typedef uint64_t uint64;

typedef int32_t int32;
typedef uint32_t uint32;

typedef int16_t int16;
typedef uint16_t uint16;

typedef int8_t int8;
typedef uint8_t uint8;

#define RESTRICT __restrict
#undef PI
#define PI                                                                     \
  (3.1415926535897932f) /* Extra digits if                                     \
                           needed: 3.1415926535897932384626433832795f */
#define SMALL_NUMBER (1.e-8f)
#define KINDA_SMALL_NUMBER (1.e-4f)
#define BIG_NUMBER (3.4e+38f)
#define EULERS_NUMBER (2.71828182845904523536f)
#define UE_GOLDEN_RATIO                                                        \
  (1.6180339887498948482045868343656381f) /* Also known as divine proportion,  \
                                             golden mean, or golden section -  \
                                             related to the Fibonacci Sequence \
                                             = (1 + sqrt(5)) / 2 */
#define FLOAT_NON_FRACTIONAL                                                   \
  (8388608.f) /* All single-precision floating point numbers greater than or   \
                 equal to this have no fractional value. */

#define THRESH_VECTOR_NORMALIZED                                               \
  (0.01f) /** Allowed error for a normalized vector (against squared           \
             magnitude) */
#define THRESH_QUAT_NORMALIZED                                                 \
  (0.01f) /** Allowed error for a normalized quaternion (against squared       \
             magnitude) */
// Copied from float.h
#define MAX_FLT 3.402823466e+38F

#if defined(_M_X64) || defined(_M_IX86)
#include "Math/UnrealMathSSE.h"
#endif

class FMatrix {
public:
  union {
    float M[4][4];
    float V[16];
  };

  FMatrix() { memset(M, 0, sizeof(FMatrix)); }
  FMatrix(float m00, float m01, float m02, float m03, float m10, float m11,
      float m12, float m13, float m20, float m21, float m22, float m23,
      float m30, float m31, float m32, float m33) {
    M[0][0] = m00;
    M[0][1] = m01;
    M[0][2] = m02;
    M[0][3] = m03;
    M[1][0] = m10;
    M[1][1] = m11;
    M[1][2] = m12;
    M[1][3] = m13;
    M[2][0] = m20;
    M[2][1] = m21;
    M[2][2] = m22;
    M[2][3] = m23;
    M[3][0] = m30;
    M[3][1] = m31;
    M[3][2] = m32;
    M[3][3] = m33;
  }
};

#if defined(__AVX__)
#include <immintrin.h> //avx2

FORCEINLINE __m256 TwoColumnComb_AVX_8(__m256 A01, const FMatrix &B) {
  __m256 Result;

  __m256 R0 = _mm256_shuffle_ps(A01, A01, 0x00);
  __m256 R1 = _mm256_shuffle_ps(A01, A01, 0x55);
  __m256 R2 = _mm256_shuffle_ps(A01, A01, 0xaa);
  __m256 R3 = _mm256_shuffle_ps(A01, A01, 0xff);

  __m256 M0 = _mm256_broadcast_ps((__m128 *)B.M[0]);
  __m256 M1 = _mm256_broadcast_ps((__m128 *)B.M[1]);
  __m256 M2 = _mm256_broadcast_ps((__m128 *)B.M[2]);
  __m256 M3 = _mm256_broadcast_ps((__m128 *)B.M[3]);

  Result = _mm256_mul_ps(R0, M0);
  Result = _mm256_fmadd_ps(R1, M1, Result);
  Result = _mm256_fmadd_ps(R2, M2, Result);
  Result = _mm256_fmadd_ps(R3, M3, Result);

  return Result;
}

// this should be noticeably faster with actual 256-bit wide vector units
// (Intel); not sure about double-pumped 128-bit (AMD), would need to check. UE4
// style
FORCEINLINE void matrixMultiplyAVX(FMatrix &out, const FMatrix &A,
                                   const FMatrix &B) {
  _mm256_zeroupper();

  __m256 A01 = _mm256_loadu_ps(A.M[0]);
  __m256 A23 = _mm256_loadu_ps(A.M[2]);

  __m256 out01x = TwoColumnComb_AVX_8(A01, B);
  __m256 out23x = TwoColumnComb_AVX_8(A23, B);

  _mm256_storeu_ps(out.M[0], out01x);
  _mm256_storeu_ps(out.M[2], out23x);
}
#endif