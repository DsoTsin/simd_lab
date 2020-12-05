#pragma once

#include <stdint.h>

namespace std
{
	template <int>
	struct TSlotMaskTypeTrait {
	};

	template<>
	struct TSlotMaskTypeTrait<8>
	{
		typedef uint8_t MaskType;
	};

	template<>
	struct TSlotMaskTypeTrait<16>
	{
		typedef uint16_t MaskType;
	};

	template<>
	struct TSlotMaskTypeTrait<32>
	{
		typedef uint32_t MaskType;
	};

	template<>
	struct TSlotMaskTypeTrait<64>
	{
		typedef uint64_t MaskType;
	};

	template <int NBits>
	struct TSlotMask
	{
		typedef typename TSlotMaskTypeTrait<NBits>::MaskType MaskType;


		void operator|=(const TSlotMask& other)		  { m |= other.m; }
		void operator&=(const TSlotMask& other)		  { m &= other.m; }
		bool operator==(const TSlotMask& other) const { return m==other.m; }
		bool operator!=(const TSlotMask& other) const { return m!=other.m; }
		operator bool() const { return m != 0; }


		MaskType m;
	};

}