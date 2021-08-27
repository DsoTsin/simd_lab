#include "CoreMinimal.h"

static FORCEINLINE bool IntersectBoxWithPermutedPlanes(
	const FConvexVolume::FPermutedPlaneArray& PermutedPlanes,
	const VectorRegister BoxOrigin,
	const VectorRegister BoxExtent)
{
	bool Result = true;

	//checkSlow(PermutedPlanes.Num() % 4 == 0);

	// Splat origin into 3 vectors
	VectorRegister OrigX = VectorReplicate(BoxOrigin, 0);
	VectorRegister OrigY = VectorReplicate(BoxOrigin, 1);
	VectorRegister OrigZ = VectorReplicate(BoxOrigin, 2);
	// Splat extent into 3 vectors
	VectorRegister ExtentX = VectorReplicate(BoxExtent, 0);
	VectorRegister ExtentY = VectorReplicate(BoxExtent, 1);
	VectorRegister ExtentZ = VectorReplicate(BoxExtent, 2);
	// Splat the abs for the pushout calculation
	VectorRegister AbsExt = VectorAbs(BoxExtent);
	VectorRegister AbsExtentX = VectorReplicate(AbsExt, 0);
	VectorRegister AbsExtentY = VectorReplicate(AbsExt, 1);
	VectorRegister AbsExtentZ = VectorReplicate(AbsExt, 2);
	// Since we are moving straight through get a pointer to the data
	const FPlane* RESTRICT PermutedPlanePtr = (FPlane*)PermutedPlanes.data();
	// Process four planes at a time until we have < 4 left
	for (size_t Count = 0, Num = PermutedPlanes.size(); Count < Num; Count += 4)
	{
		// Load 4 planes that are already all Xs, Ys, ...
		VectorRegister PlanesX = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesY = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesZ = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesW = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		// Calculate the distance (x * x) + (y * y) + (z * z) - w
		VectorRegister DistX = VectorMultiply(OrigX, PlanesX);
		VectorRegister DistY = VectorMultiplyAdd(OrigY, PlanesY, DistX);
		VectorRegister DistZ = VectorMultiplyAdd(OrigZ, PlanesZ, DistY);
		VectorRegister Distance = VectorSubtract(DistZ, PlanesW);
		// Now do the push out FMath::Abs(x * x) + FMath::Abs(y * y) + FMath::Abs(z * z)
		VectorRegister PushX = VectorMultiply(AbsExtentX, VectorAbs(PlanesX));
		VectorRegister PushY = VectorMultiplyAdd(AbsExtentY, VectorAbs(PlanesY), PushX);
		VectorRegister PushOut = VectorMultiplyAdd(AbsExtentZ, VectorAbs(PlanesZ), PushY);

		// Check for completely outside
		if (VectorAnyGreaterThan(Distance, PushOut))
		{
			Result = false;
			break;
		}
	}
	return Result;
}

bool FConvexVolume::IntersectBox(const FVector& Origin, const FVector& Extent) const
{
	// Load the origin & extent
	const VectorRegister Orig = VectorLoadFloat3(&Origin);
	const VectorRegister Ext = VectorLoadFloat3(&Extent);
	return IntersectBoxWithPermutedPlanes(PermutedPlanes, Orig, Ext);
}

//bool FConvexVolume::IntersectBox(const FVector& Origin, const FVector& Translation, const FVector& Extent) const
//{
//	const VectorRegister Orig = VectorLoadFloat3(&Origin);
//	const VectorRegister Trans = VectorLoadFloat3(&Translation);
//	const VectorRegister BoxExtent = VectorLoadFloat3(&Extent);
//	const VectorRegister BoxOrigin = VectorAdd(Orig, Trans);
//	return IntersectBoxWithPermutedPlanes(PermutedPlanes, BoxOrigin, BoxExtent);
//}

bool FConvexVolume::IntersectBox(const FVector& Origin, const FVector& Extent, bool& bOutFullyContained) const
{
	bool Result = true;

	// Assume fully contained
	bOutFullyContained = true;

	//checkSlow(PermutedPlanes.Num() % 4 == 0);

	// Load the origin & extent
	VectorRegister Orig = VectorLoadFloat3(&Origin);
	VectorRegister Ext = VectorLoadFloat3(&Extent);
	// Splat origin into 3 vectors
	VectorRegister OrigX = VectorReplicate(Orig, 0);
	VectorRegister OrigY = VectorReplicate(Orig, 1);
	VectorRegister OrigZ = VectorReplicate(Orig, 2);
	// Splat extent into 3 vectors
	VectorRegister ExtentX = VectorReplicate(Ext, 0);
	VectorRegister ExtentY = VectorReplicate(Ext, 1);
	VectorRegister ExtentZ = VectorReplicate(Ext, 2);
	// Splat the abs for the pushout calculation
	VectorRegister AbsExt = VectorAbs(Ext);
	VectorRegister AbsExtentX = VectorReplicate(AbsExt, 0);
	VectorRegister AbsExtentY = VectorReplicate(AbsExt, 1);
	VectorRegister AbsExtentZ = VectorReplicate(AbsExt, 2);
	// Since we are moving straight through get a pointer to the data
	const FPlane* RESTRICT PermutedPlanePtr = (FPlane*)PermutedPlanes.data();
	// Process four planes at a time until we have < 4 left
	for (size_t Count = 0; Count < PermutedPlanes.size(); Count += 4)
	{
		// Load 4 planes that are already all Xs, Ys, ...
		VectorRegister PlanesX = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesY = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesZ = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesW = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		// Calculate the distance (x * x) + (y * y) + (z * z) - w
		VectorRegister DistX = VectorMultiply(OrigX, PlanesX);
		VectorRegister DistY = VectorMultiplyAdd(OrigY, PlanesY, DistX);
		VectorRegister DistZ = VectorMultiplyAdd(OrigZ, PlanesZ, DistY);
		VectorRegister Distance = VectorSubtract(DistZ, PlanesW);
		// Now do the push out FMath::Abs(x * x) + FMath::Abs(y * y) + FMath::Abs(z * z)
		VectorRegister PushX = VectorMultiply(AbsExtentX, VectorAbs(PlanesX));
		VectorRegister PushY = VectorMultiplyAdd(AbsExtentY, VectorAbs(PlanesY), PushX);
		VectorRegister PushOut = VectorMultiplyAdd(AbsExtentZ, VectorAbs(PlanesZ), PushY);
		VectorRegister PushOutNegative = VectorNegate(PushOut);

		// Check for completely outside
		if (VectorAnyGreaterThan(Distance, PushOut))
		{
			Result = false;
			bOutFullyContained = false;
			break;
		}

		// Definitely inside frustums, but check to see if it's fully contained
		if (VectorAnyGreaterThan(Distance, PushOutNegative))
		{
			bOutFullyContained = false;
		}
	}
	return Result;
}

//
//	FConvexVolume::IntersectSphere
//

bool FConvexVolume::IntersectSphere(const FVector& Origin, const float& Radius) const
{
	bool Result = true;

	//checkSlow(PermutedPlanes.Num() % 4 == 0);

	// Load the origin & radius
	VectorRegister Orig = VectorLoadFloat3(&Origin);
	VectorRegister VRadius = VectorLoadFloat1(&Radius);
	// Splat origin into 3 vectors
	VectorRegister OrigX = VectorReplicate(Orig, 0);
	VectorRegister OrigY = VectorReplicate(Orig, 1);
	VectorRegister OrigZ = VectorReplicate(Orig, 2);
	// Since we are moving straight through get a pointer to the data
	const FPlane* RESTRICT PermutedPlanePtr = (FPlane*)PermutedPlanes.data();
	// Process four planes at a time until we have < 4 left
	for (int32 Count = 0; Count < PermutedPlanes.size(); Count += 4)
	{
		// Load 4 planes that are already all Xs, Ys, ...
		VectorRegister PlanesX = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesY = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesZ = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesW = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		// Calculate the distance (x * x) + (y * y) + (z * z) - w
		VectorRegister DistX = VectorMultiply(OrigX, PlanesX);
		VectorRegister DistY = VectorMultiplyAdd(OrigY, PlanesY, DistX);
		VectorRegister DistZ = VectorMultiplyAdd(OrigZ, PlanesZ, DistY);
		VectorRegister Distance = VectorSubtract(DistZ, PlanesW);

		// Check for completely outside
		if (VectorAnyGreaterThan(Distance, VRadius))
		{
			Result = false;
			break;
		}
	}
	return Result;
}

bool FConvexVolume::IntersectSphere(const FVector& Origin, const float& Radius, bool& bOutFullyContained) const
{
	bool Result = true;

	//Assume fully contained
	bOutFullyContained = true;

	//checkSlow(PermutedPlanes.Num() % 4 == 0);

	// Load the origin & radius
	VectorRegister Orig = VectorLoadFloat3(&Origin);
	VectorRegister VRadius = VectorLoadFloat1(&Radius);
	VectorRegister NegativeVRadius = VectorNegate(VRadius);
	// Splat origin into 3 vectors
	VectorRegister OrigX = VectorReplicate(Orig, 0);
	VectorRegister OrigY = VectorReplicate(Orig, 1);
	VectorRegister OrigZ = VectorReplicate(Orig, 2);
	// Since we are moving straight through get a pointer to the data
	const FPlane* RESTRICT PermutedPlanePtr = (FPlane*)PermutedPlanes.data();
	// Process four planes at a time until we have < 4 left
	for (int32 Count = 0; Count < PermutedPlanes.size(); Count += 4)
	{
		// Load 4 planes that are already all Xs, Ys, ...
		VectorRegister PlanesX = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesY = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesZ = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesW = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		// Calculate the distance (x * x) + (y * y) + (z * z) - w
		VectorRegister DistX = VectorMultiply(OrigX, PlanesX);
		VectorRegister DistY = VectorMultiplyAdd(OrigY, PlanesY, DistX);
		VectorRegister DistZ = VectorMultiplyAdd(OrigZ, PlanesZ, DistY);
		VectorRegister Distance = VectorSubtract(DistZ, PlanesW);

		// Check for completely outside
		int32 Mask = VectorAnyGreaterThan(Distance, VRadius);
		if (Mask)
		{
			Result = false;
			bOutFullyContained = false;
			break;
		}

		//the sphere is definitely inside the frustums, but let's check if it's FULLY contained by checking the NEGATIVE radius (on the inside of each frustum plane)
		Mask = VectorAnyGreaterThan(Distance, NegativeVRadius);
		if (Mask)
		{
			bOutFullyContained = false;
		}
	}
	return Result;
}