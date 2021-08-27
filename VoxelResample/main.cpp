// Copyright Lilith Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "ScopeTimer.h"

//#include "VoxelResample.ispc.h"
#include "VoxelResample.ispc_avx512skx.h"

struct FBrickDescription
{
	float MaxDim;
	FVector Anchor;
	FVector Extent;
};

struct FBox
{
public:
	/** Holds the box's minimum point. */
	FVector Min;
	/** Holds the box's maximum point. */
	FVector Max;
	/** Holds a flag indicating whether this box is valid. */
	uint8 IsValid;

	FVector GetCenter() const { return ( Min + Max ) * FVector{ 0.5f, 0.5f, 0.5f}; }
	FVector GetExtent() const { return ( Max - Min ) * FVector{ 0.5f, 0.5f, 0.5f}; }
	
	__forceinline bool IsInside( const FVector& In ) const
	{
		return ((In.X > Min.X) && (In.X < Max.X) && (In.Y > Min.Y) && (In.Y < Max.Y) && (In.Z > Min.Z) && (In.Z < Max.Z));
	}
};

struct FPackedVoxel
{
	uint32 AlbedoOpacity;
	uint32 Normal;
	uint32 Emissive;
	uint32 PositionPacked;
};

uint3 UnpackPositionR11G11B10(uint32 PackedPosition)
{
	return uint3{PackedPosition >> 21, (PackedPosition >> 10) & 0x7ff, PackedPosition & 0x3ff};
}

uint32 PackPositionR11G11B10(uint3 Position)
{
	return uint32((Position.X & 0x7ff) << 21 | (Position.Y & 0x7ff) << 10 | (Position.Z & 0x3ff));
}

uint32 PackPositionR11G11B10(int32 X, int32 Y, int32 Z)
{
	return uint32((X & 0x7ff) << 21 | (Y & 0x7ff) << 10 | (Z & 0x3ff));
}

inline FVector GetVoxelWorldPos(uint3 VoxelPosition, float MaxDim, FVector InExtent, FVector InAnchor)
{
	FVector InvMaxDim(1.f / MaxDim);
	return (FVector(1.f * VoxelPosition.X, 1.f * VoxelPosition.Y, 1.f * VoxelPosition.Z) * InvMaxDim * FVector(2.f) - FVector(1.f)) * InExtent + InAnchor;
}

FBox testBox{
	FVector{34128,-62113,10211},
	FVector{75156,-42153,40818},
	1
};

typedef std::vector<FPackedVoxel> FVoxelData;

void resample(const FVoxelData& InVoxels, const FBrickDescription& Description, FVoxelData& NewVoxels);

int main(int argc, const char* argv[])
{
	std::ifstream vfile("voxel_43_10_3.data", std::ios::in | std::ios::binary);
	assert(vfile.good());
	FBox box;
	uint3 res;
	uint3 pos;
	uint3 minCoord;
	uint3 maxCoord;
	int32 numVoxels;
	vfile.read((char*)&box.Min, sizeof(box.Min));
	vfile.read((char*)&box.Max, sizeof(box.Max));
	vfile.read((char*)&box.IsValid, sizeof(box.IsValid));
	vfile.read((char*)&res, sizeof(res));
	vfile.read((char*)&pos, sizeof(pos));
	vfile.read((char*)&minCoord, sizeof(minCoord));
	vfile.read((char*)&maxCoord, sizeof(maxCoord));
	vfile.read((char*)&numVoxels, sizeof(numVoxels));
	std::vector<FPackedVoxel> voxels(numVoxels);
	vfile.read((char*)voxels.data(), sizeof(FPackedVoxel) * numVoxels);
	FBrickDescription desc;
	desc.Anchor = box.GetCenter();
	desc.Extent = box.GetExtent();
	desc.MaxDim = 512;

	FVector MinWs = GetVoxelWorldPos(minCoord, 512, desc.Extent, desc.Anchor);
	FVector MaxWs = GetVoxelWorldPos(maxCoord, 512, desc.Extent, desc.Anchor);

	assert(sizeof(desc) == sizeof(ispc::FBrickDescription));

	
	std::vector<FPackedVoxel> new_voxels;
	new_voxels.reserve(numVoxels);
	std::vector<FPackedVoxel> new_voxels2;
	new_voxels2.reserve(numVoxels);

	std::vector<uint8> flags;
	flags.resize(numVoxels);
	std::vector<FPackedVoxel> new_voxels3;
	new_voxels3.resize(numVoxels);

	std::vector<FPackedVoxel> new_voxels4;
	new_voxels4.reserve(numVoxels);

	std::cout<< "numVoxels = " << numVoxels << std::endl;

	{
		ScopeTimer _("renderFrameISPC0");
		ispc::Resample(numVoxels, (const ispc::FPackedVoxel*)voxels.data(), (const ispc::FBrickDescription&)desc, 
			(const ispc::FVector3f&)testBox.Min, (const ispc::FVector3f&)testBox.Max, {512,512,256}, 
			(uint8*)flags.data(), (ispc::FPackedVoxel*)new_voxels3.data());
		for (int32 index = 0; index < numVoxels; index++)
		{
			if (flags[index])
			{
				new_voxels4.push_back(new_voxels3[index]);
			}
		}
	}

	{
		ScopeTimer _("renderFrameISPC");
		ispc::ResampleMasked(numVoxels, (const ispc::FPackedVoxel*)voxels.data(), (const ispc::FBrickDescription&)desc,
			(const ispc::FVector3f&)testBox.Min, (const ispc::FVector3f&)testBox.Max, {512,512,256}, 
			[](void* userData, const ispc::FPackedVoxel& v, int index) {
				std::vector<FPackedVoxel>* ptrVoxels = (std::vector<FPackedVoxel>*)userData;
				ptrVoxels->push_back((const FPackedVoxel&)v);
			},
			&new_voxels);
	}

	{
		ScopeTimer _("renderFrameScalar");
		resample(voxels, desc, new_voxels2);
	}

	assert(new_voxels.size() == new_voxels2.size());
	assert(new_voxels.size() == new_voxels4.size());

	return 0;
}

uint3 GridDims{512,512,256};

void resample(const FVoxelData& InVoxels, const FBrickDescription& Description, FVoxelData& NewVoxels)
{
	FVector MyCenter = testBox.GetCenter();
	FVector MyExtent = testBox.GetExtent();
	for(auto& Voxel: InVoxels)
	{
		FVector WorldPosition = GetVoxelWorldPos(UnpackPositionR11G11B10(Voxel.PositionPacked), Description.MaxDim, Description.Extent, Description.Anchor);
		if (testBox.IsInside(WorldPosition))
		{
			FVector Quantized = ((WorldPosition - MyCenter) / MyExtent + 1.0f) * 0.5f; // [-1, 1]
			FVector NewCoordF = Quantized * FVector(GridDims);
			FPackedVoxel v;
			v.PositionPacked = PackPositionR11G11B10( FMath::FloorToInt(NewCoordF.X), FMath::FloorToInt(NewCoordF.Y), FMath::FloorToInt(NewCoordF.Z) );
			v.AlbedoOpacity = Voxel.AlbedoOpacity;
			v.Emissive = Voxel.Emissive;
			v.Normal = Voxel.Normal;

			NewVoxels.push_back(v);
		}
	}
}