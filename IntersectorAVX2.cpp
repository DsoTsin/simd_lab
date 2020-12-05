
#include <stdint.h>
#include <immintrin.h> //avx2

#include "SlotMaskOps.ispc.h"

#include <assert.h>

static __m256i v0_m = _mm256_setr_epi32(0, 0, 0, 0, 0, 0, 0, 0);
static __m256i v1_m = _mm256_setr_epi32(1, 1, 1, 1, 1, 1, 1, 1);
static __m256i v2_m = _mm256_setr_epi32(2, 2, 2, 2, 2, 2, 2, 2);
static __m256 signm = _mm256_set1_ps(-0.0);

__forceinline __m256 _mm256_abs(__m256 i)
{
	return _mm256_andnot_ps(signm, i);
}

bool intersect(float* origin, float* extent, float* permutated_plane)
{
	bool Result = true;

	__m256 O = _mm256_loadu_ps(origin);
	__m256 E = _mm256_loadu_ps(extent);

	__m256 OX = _mm256_permutevar8x32_ps(O, v0_m);
	__m256 OY = _mm256_permutevar8x32_ps(O, v1_m);
	__m256 OZ = _mm256_permutevar8x32_ps(O, v2_m);

	__m256 EX = _mm256_permutevar8x32_ps(E, v0_m);
	__m256 EY = _mm256_permutevar8x32_ps(E, v1_m);
	__m256 EZ = _mm256_permutevar8x32_ps(E, v2_m);

	__m256 PX = _mm256_load_ps(permutated_plane);
	__m256 PY = _mm256_load_ps(permutated_plane + 8);
	__m256 PZ = _mm256_load_ps(permutated_plane + 16);
	__m256 PW = _mm256_load_ps(permutated_plane + 24);

	__m256 AbsPX = _mm256_abs(PX);
	__m256 AbsPY = _mm256_abs(PY);
	__m256 AbsPZ = _mm256_abs(PZ);

	__m256 DX = _mm256_mul_ps(OX, PX);
	__m256 DY = _mm256_fmadd_ps(OY, PY, DX);
	__m256 DZ = _mm256_fmadd_ps(OZ, PZ, DY);
	__m256 D = _mm256_sub_ps(DZ, PW);

	__m256 PushX = _mm256_mul_ps(EX, AbsPX);
	__m256 PushY = _mm256_fmadd_ps(EY, AbsPY, PushX);
	__m256 PushOut = _mm256_fmadd_ps(EZ, AbsPZ, PushY);

	__m256 cmp = _mm256_cmp_ps(D, PushOut, _CMP_GT_OQ);
	int mask = _mm256_movemask_ps(cmp);

	if (mask)
	{
		Result = false;
	}

	return Result;
}

class FBitMask
{
public:
	FBitMask();
	~FBitMask();

	void		init(uint32_t slotCount);

	bool		anySlotUsed() const;

	void		markUsed(uint32_t index);
	void		markUnused(uint32_t index);
	uint32_t	maxUsedSlot() const;

	uint32_t	length() const { return length_; }

	operator bool() const {return true;}

private:
	uint32_t	length_;
	uint32_t*	mask_;
};

FBitMask::FBitMask()
: length_(0)
, mask_(nullptr)
{
}

FBitMask::~FBitMask()
{
	if (mask_) 
	{
		::_aligned_free(mask_);
		mask_ = nullptr;
	}
	length_ = 0;
}

void FBitMask::init(uint32_t slotCount)
{
	uint32_t bytes = slotCount / 8;
	if (bytes == 0)
	{
		bytes = 1;
	}
	length_ = slotCount;
	// 32 byte align
	mask_ = reinterpret_cast<uint32_t*>(::_aligned_malloc(bytes, 32));
	// set zeros
	ispc::test();
}

bool FBitMask::anySlotUsed() const
{
	return false;
}

void FBitMask::markUsed(uint32_t index)
{
}

void FBitMask::markUnused(uint32_t index)
{
}

uint32_t FBitMask::maxUsedSlot() const
{
	return uint32_t();
}

