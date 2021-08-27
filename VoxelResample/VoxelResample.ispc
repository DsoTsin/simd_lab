// Copyright Lilith Games, Inc. All Rights Reserved.

#include "Vector.isph"

struct FBrickDescription
{
	float MaxDim;
	FVector3f Anchor;
	FVector3f Extent;
};

struct FPackedVoxel
{
	uint32 AlbedoOpacity;
	uint32 Normal;
	uint32 Emissive;
	uint32 PositionPacked;
};

inline FIntVector UnpackPositionR11G11B10(uint32 PackedPosition)
{
	FIntVector result;
	result.V[0] = PackedPosition >> 21;
	result.V[1] = (PackedPosition >> 10) & 0x7ff;
	result.V[2] = PackedPosition & 0x3ff;
	return result;
}

inline uint32 PackPositionR11G11B10(FVector3f Position)
{
	return ((int32)Position.V[0] & 0x7ff) << 21 | ((int32)Position.V[1] & 0x7ff) << 10 | ((int32)Position.V[2] & 0x3ff);
}

inline FVector3f GetVoxelWorldPos(FIntVector VoxelPosition, float InvMaxDim, FVector3f InExtent, FVector3f InAnchor)
{
	varying float x = (float)(VoxelPosition.V[0]) * InvMaxDim * 2.f - 1.f;
	varying float y = (float)(VoxelPosition.V[1]) * InvMaxDim * 2.f - 1.f;
	varying float z = (float)(VoxelPosition.V[2]) * InvMaxDim * 2.f - 1.f;
	FVector3f Result = SetVector(x, y, z);
	return Result * InExtent + InAnchor;
}

inline varying FPackedVoxel VoxelLoad(const uniform FPackedVoxel *uniform SrcPtr)
{
	varying FPackedVoxel Result;

	if(((1<<TARGET_WIDTH)-1 ^ lanemask()) == 0)
	{
		aos_to_soa4_ispc(
			(uniform int32 * uniform)SrcPtr, 
			(varying int32* uniform)&Result.AlbedoOpacity, 
			(varying int32* uniform)&Result.Normal, 
			(varying int32* uniform)&Result.Emissive, 
			(varying int32* uniform)&Result.PositionPacked);
	}
	else
	{
		#pragma ignore warning(perf)
		Result = SrcPtr[programIndex];
	}

	return Result;
}

inline void VoxelStore(uniform FPackedVoxel *uniform DstPtr, const varying FPackedVoxel &Vx)
{
	if(((1<<TARGET_WIDTH)-1 ^ lanemask()) == 0)
	{
		soa_to_aos4_ispc((int32)Vx.AlbedoOpacity, (int32)Vx.Normal, (int32)Vx.Emissive, (int32)Vx.PositionPacked, (uniform int32 *uniform)DstPtr);
	}
	else
	{
		#pragma ignore warning(perf)
		DstPtr[programIndex] = Vx;
	}
}

export void Resample(
	const uniform int32 NumVoxels, 
	const uniform FPackedVoxel InVoxels[], 
	const uniform FBrickDescription& Desc, 
	const uniform FVector3f& MinWS, 
	const uniform FVector3f& MaxWS, 
	const uniform FIntVector& NewGridDims, 
	uniform uint8 Flags[],
	uniform FPackedVoxel OutVoxels[]
) {
	float InvMaxDim = rcp(Desc.MaxDim);
	FVector3f NewCenter = (MinWS + MaxWS) * FloatHalfVector;
	FVector3f NewExtent = (MaxWS - MinWS) * FloatHalfVector;
	FVector3f GridDims = SetVector( 1.f * NewGridDims.V[0], 1.f * NewGridDims.V[1], 1.f * NewGridDims.V[2] );
	foreach(i = 0 ... NumVoxels)
	{
		const uniform int Index = extract(i, 0);
		const FPackedVoxel Voxel = VoxelLoad(&InVoxels[Index]);
		FIntVector PositionGS = UnpackPositionR11G11B10(Voxel.PositionPacked);
		FVector3f WorldPos = GetVoxelWorldPos(PositionGS, InvMaxDim, Desc.Extent, Desc.Anchor);
		
		uint8 bIsInside = (WorldPos.V[0] >= MinWS.V[0] && WorldPos.V[0] <= MaxWS.V[0]
		 && WorldPos.V[1] >= MinWS.V[1] && WorldPos.V[1] <= MaxWS.V[1]
		 && WorldPos.V[2] >= MinWS.V[2] && WorldPos.V[2] <= MaxWS.V[2]);
		 
		FVector3f NewCoord = (( WorldPos - NewCenter ) / NewExtent + FloatOneVector ) * FloatHalfVector * GridDims;

		uint32 PackedCoord = PackPositionR11G11B10(NewCoord);
		FPackedVoxel NewVoxel;
		NewVoxel.AlbedoOpacity = Voxel.AlbedoOpacity;
		NewVoxel.Normal = Voxel.Normal;
		NewVoxel.Emissive = Voxel.Emissive;
		NewVoxel.PositionPacked = PackedCoord;
		
		VoxelStore(&OutVoxels[Index], NewVoxel);
		Flags[Index + programIndex] = bIsInside;
	}
}


typedef unmasked void (*OutputVoxels)(void* uniform self, const uniform FPackedVoxel& vx, const uniform int index);

export void ResampleMasked(
	const uniform int32 NumVoxels, 
	const uniform FPackedVoxel InVoxels[], 
	const uniform FBrickDescription& Desc, 
	const uniform FVector3f& MinWS, 
	const uniform FVector3f& MaxWS, 
	const uniform FIntVector& NewGridDims, 
	uniform OutputVoxels fnOutput,
	void* uniform userData
) {
	float InvMaxDim = rcp(Desc.MaxDim);
	FVector3f NewCenter = (MinWS + MaxWS) * FloatHalfVector;
	FVector3f NewExtent = (MaxWS - MinWS) * FloatHalfVector;
	FVector3f GridDims = SetVector( 1.f * NewGridDims.V[0], 1.f * NewGridDims.V[1], 1.f * NewGridDims.V[2] );
	uniform uint32 OutputIndex = 0;
	foreach(i = 0 ... NumVoxels)
	{
		const uniform int Index = extract(i, 0);
		const FPackedVoxel Voxel = VoxelLoad(&InVoxels[Index]);
		FIntVector PositionGS = UnpackPositionR11G11B10(Voxel.PositionPacked);
		FVector3f WorldPos = GetVoxelWorldPos(PositionGS, InvMaxDim, Desc.Extent, Desc.Anchor);
		
		int32 bIsInside = (WorldPos.V[0] >= MinWS.V[0] && WorldPos.V[0] <= MaxWS.V[0]
		 && WorldPos.V[1] >= MinWS.V[1] && WorldPos.V[1] <= MaxWS.V[1]
		 && WorldPos.V[2] >= MinWS.V[2] && WorldPos.V[2] <= MaxWS.V[2]);
		 
		FVector3f NewCoord = (( WorldPos - NewCenter ) / NewExtent + FloatOneVector ) * FloatHalfVector * GridDims;
		uint32 PackedCoord = PackPositionR11G11B10(NewCoord);

		foreach_active(j)
		{
			unmasked
			{
				uniform int32 insideBox = extract(bIsInside, j);
				if (insideBox)
				{
					uniform FPackedVoxel v;
					v.AlbedoOpacity = extract(Voxel.AlbedoOpacity, j);
					v.Normal = extract(Voxel.Normal, j);
					v.Emissive = extract(Voxel.Emissive, j);
					v.PositionPacked = extract(PackedCoord, j);
					fnOutput(userData, v, OutputIndex);
					OutputIndex++;
				}
			}
		}
	}
}