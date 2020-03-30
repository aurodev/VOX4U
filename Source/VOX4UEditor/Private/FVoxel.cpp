// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.

#include "FVoxel.h"
#include <Engine/Texture2D.h>
#include "MonotoneMesh.h"
#include "VoxImportOption.h"
#include "vox.h"

DEFINE_LOG_CATEGORY_STATIC(LogVox, Log, All)

/**
 * Create empty vox data.
 */
FVoxel::FVoxel()
{
}

/**
 * Create vox data from archive
 * @param Filename Filename
 * @param Data Buffer to use as the source data to read from
 * @param Size Size of Data
 * @param ImportOption
 */
FVoxel::FVoxel(const FString& Filename, const void* Data, int64 Size, const UVoxImportOption* ImportOption)
{
	this->Filename = Filename;
	this->ImportOption = ImportOption;
	auto vox = vox::read(Data, Size);

	const auto& size = vox.size[0];
	this->Size.X = ImportOption->bImportXForward ? size.y : size.x;
	this->Size.Y = ImportOption->bImportXForward ? size.x : size.y;
	this->Size.Z = size.z;

	const auto& voxel = vox.voxel[0];
	for (auto n = 0; n < voxel.voxels.size(); ++n) {
		const auto& cell = voxel.voxels[n];
		auto vector = ImportOption->bImportXForward
			? FIntVector(this->Size.X - cell.y - 1, this->Size.Y - cell.x - 1, cell.z)
			: FIntVector(this->Size.X - cell.x - 1, cell.y, cell.z);
		Voxel.Add(MoveTemp(vector), cell.i);
	}
	const auto& palette = vox.palette.palettes;
	if (palette.size() != 0) {
		for (auto i = 0; i < 256; ++i) {
			const auto& color = palette[i];
			Palette.Add(FColor(color.r, color.g, color.b, color.a));
		}
	} else {
		for (auto i = 0; i < 256; ++i) {
			Palette.Add(FColor(vox::vox::default_palette[i]));
		}
	}
}

/**
 * UE4
 * FVector::UpVector(0.0f, 0.0f, 1.0f);
 * FVector::ForwardVector(1.0f, 0.0f, 0.0f);
 * FVector::RightVector(0.0f, 1.0f, 0.0f);
 *
 *   z (up)
 *   |
 *   +---x (front)
 *  /
 * y (right)
 *
 * MagicaVoxel
 * FVector::UpVector(0.0f, 0.0f, 1.0f);
 * FVector::ForwardVector(0.0f, -1.0f, 0.0f);
 * FVector::RightVector(-1.0f, 0.0f, 0.0f);
 *
 *   z (up)
 *   |
 *   | y (back)
 *   |/
 *   +---x (left)
 */
static const FIntVector Vectors[6] = {
	FIntVector( 0, 0, 1),	// Up
	FIntVector( 0, 0,-1),	// Down
	FIntVector( 1, 0, 0),	// Forward
	FIntVector(-1, 0, 0),	// Backward
	FIntVector( 0, 1, 0),	// Right
	FIntVector( 0,-1, 0)	// Left
};

/**
 *   4---5
 *  /|  /|
 * 7---6 |
 * | 0-|-1
 * |/  |/
 * 3---2
 */
static const FVector Vertexes[8] = {
	FVector(0, 0, 0),
	FVector(1, 0, 0),
	FVector(1, 1, 0),
	FVector(0, 1, 0),
	FVector(0, 0, 1),
	FVector(1, 0, 1),
	FVector(1, 1, 1),
	FVector(0, 1, 1),
};

/**
 *         7 - 4
 *         | U |
 * 4 - 7 - 6 - 5 - 4
 * | B | R | F | L |
 * 0 - 3 - 2 - 1 - 0
 *         | D |
 *         3 - 0
 */
static const uint32 Faces[6][4] = {
	{ 5, 4, 7, 6 },	// Up 		FCell( 0, 0, 1)
	{ 0, 1, 2, 3 },	// Down 	FCell( 0, 0,-1)
	{ 5, 6, 2, 1 },	// Forward 	FCell( 1, 0, 0)
	{ 7, 4, 0, 3 },	// Backward FCell(-1, 0, 0)
	{ 6, 7, 3, 2 },	// Right 	FCell( 0, 1, 0)
	{ 4, 5, 1, 0 }	// Left 	FCell( 0,-1, 0)
};

/**
 * 2 <- 1
 * | \  ^
 * .  \ |
 * 3 -> 0
 */
static const uint32 Polygons[2][3] = {
	{ 0, 1, 2 },
	{ 2, 3, 0 },
};

/**
 * 2 <- 1
 * | \  ^
 * .  \ |
 * 3 -> 0
 */
static FVector2D TextureCoord[2][3] = {
	{ FVector2D(1.f, 1.f), FVector2D(1.f, 0.f), FVector2D(0.f, 0.f) },
	{ FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f) },
};

/**
 * CreateRawMesh
 * @param FRawMesh& RawMesh	Out RawMesh
 * @return Result
 */
bool FVoxel::CreateRawMesh(FRawMesh& OutRawMesh) const
{
	for (const auto& Cell : Voxel) {
		FVector Origin(Cell.Key.X, Cell.Key.Y, Cell.Key.Z);
		for (int FaceIndex = 0; FaceIndex < 6; ++FaceIndex) {
			const auto n = Cell.Key + Vectors[FaceIndex];
			if (Voxel.Find(n)) continue;

			TArray<uint32> VertexPositionIndex;
			for (int VertexIndex = 0; VertexIndex < 4; ++VertexIndex) {
				FVector v = Origin + Vertexes[Faces[FaceIndex][VertexIndex]];
				int32 vpi = OutRawMesh.VertexPositions.AddUnique(v);
				VertexPositionIndex.Add(vpi);
			}

			uint8 ColorIndex = Cell.Value - 1;
			for (int PolygonIndex = 0; PolygonIndex < 2; ++PolygonIndex) {
				OutRawMesh.WedgeIndices.Add(VertexPositionIndex[Polygons[PolygonIndex][0]]);
				OutRawMesh.WedgeIndices.Add(VertexPositionIndex[Polygons[PolygonIndex][1]]);
				OutRawMesh.WedgeIndices.Add(VertexPositionIndex[Polygons[PolygonIndex][2]]);
				OutRawMesh.WedgeColors.Add(Palette[ColorIndex]);
				OutRawMesh.WedgeColors.Add(Palette[ColorIndex]);
				OutRawMesh.WedgeColors.Add(Palette[ColorIndex]);
				OutRawMesh.WedgeTexCoords[0].Add(FVector2D(((double)ColorIndex + 0.5) / 256.0, 0.5));
				OutRawMesh.WedgeTexCoords[0].Add(FVector2D(((double)ColorIndex + 0.5) / 256.0, 0.5));
				OutRawMesh.WedgeTexCoords[0].Add(FVector2D(((double)ColorIndex + 0.5) / 256.0, 0.5));
				OutRawMesh.FaceMaterialIndices.Add(0);
				OutRawMesh.FaceSmoothingMasks.Add(0);
			}
		}
	}

	FVector Offset = ImportOption->bImportXYCenter ? FVector((float)Size.X * 0.5f, (float)Size.Y * 0.5f, 0.f) : FVector::ZeroVector;
	for (int32 i = 0; i < OutRawMesh.VertexPositions.Num(); ++i) {
		FVector VertexPosition = OutRawMesh.VertexPositions[i];
		OutRawMesh.VertexPositions[i] = VertexPosition - Offset;
	}

	OutRawMesh.CompactMaterialIndices();
	check(OutRawMesh.IsValidOrFixable());

	return true;
}

/**
 * CreateOptimizedRawMesh
 * @param OutRawMesh Out raw mesh
 * @return Result
 */
bool FVoxel::CreateOptimizedRawMesh(FRawMesh& OutRawMesh) const
{
	MonotoneMesh Mesher(this);
	return Mesher.CreateRawMesh(OutRawMesh, ImportOption);
}

/**
 * CreateRawMesh
 * @param FRawMesh& RawMesh	Out RawMesh
 * @return Result
 */
bool FVoxel::CreateRawMeshes(TArray<FRawMesh>& OutRawMeshes) const
{
	for (const auto& Cell : Voxel) {
		FRawMesh OutRawMesh;

		FVector Origin(Cell.Key.X, Cell.Key.Y, Cell.Key.Z);
		for (int VertexIndex = 0; VertexIndex < 8; ++VertexIndex) {
			OutRawMesh.VertexPositions.Add(Origin + Vertexes[VertexIndex]);
		}
		for (int FaceIndex = 0; FaceIndex < 6; ++FaceIndex) {
			uint8 ColorIndex = Cell.Value - 1;
			for (int PolygonIndex = 0; PolygonIndex < 2; ++PolygonIndex) {
				OutRawMesh.WedgeIndices.Add(Faces[FaceIndex][Polygons[PolygonIndex][0]]);
				OutRawMesh.WedgeIndices.Add(Faces[FaceIndex][Polygons[PolygonIndex][1]]);
				OutRawMesh.WedgeIndices.Add(Faces[FaceIndex][Polygons[PolygonIndex][2]]);
				OutRawMesh.WedgeColors.Add(Palette[ColorIndex]);
				OutRawMesh.WedgeColors.Add(Palette[ColorIndex]);
				OutRawMesh.WedgeColors.Add(Palette[ColorIndex]);
				OutRawMesh.WedgeTexCoords[0].Add(FVector2D(((double)ColorIndex + 0.5) / 256.0, 0.5));
				OutRawMesh.WedgeTexCoords[0].Add(FVector2D(((double)ColorIndex + 0.5) / 256.0, 0.5));
				OutRawMesh.WedgeTexCoords[0].Add(FVector2D(((double)ColorIndex + 0.5) / 256.0, 0.5));
				OutRawMesh.FaceMaterialIndices.Add(0);
				OutRawMesh.FaceSmoothingMasks.Add(0);
			}
		}
		OutRawMeshes.Add(OutRawMesh);
	}

	FVector Offset = ImportOption->bImportXYCenter ? FVector((float)Size.X * 0.5f, (float)Size.Y * 0.5f, 0.f) : FVector::ZeroVector;
	for (FRawMesh& OutRawMesh : OutRawMeshes) {
		for (int32 i = 0; i < OutRawMesh.VertexPositions.Num(); ++i) {
			FVector VertexPosition = OutRawMesh.VertexPositions[i];
			OutRawMesh.VertexPositions[i] = VertexPosition - Offset;
		}
		OutRawMesh.CompactMaterialIndices();
		check(OutRawMesh.IsValidOrFixable());
	}

	return true;
}


bool FVoxel::CreateTexture(UTexture2D* const& OutTexture) const
{
	check(OutTexture);
	OutTexture->LODGroup = TextureGroup::TEXTUREGROUP_World;
	OutTexture->CompressionSettings = TextureCompressionSettings::TC_Default;
	OutTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	OutTexture->DeferCompression = true;
	OutTexture->Source.Init(256, 1, 1, 1, TSF_BGRA8, (const uint8*)Palette.GetData());
	OutTexture->UpdateResource();
	OutTexture->PostEditChange();
	return true;
}

bool FVoxel::CreateMesh(FRawMesh& OutRawMesh, const UVoxImportOption* ImportOption)
{
	for (int VertexIndex = 0; VertexIndex < 8; ++VertexIndex) {
		OutRawMesh.VertexPositions.Add(Vertexes[VertexIndex] - FVector(0.5f, 0.5f, 0.5f));
	}
	for (int FaceIndex = 0; FaceIndex < 6; ++FaceIndex) {
		for (int PolygonIndex = 0; PolygonIndex < 2; ++PolygonIndex) {
			OutRawMesh.WedgeIndices.Add(Faces[FaceIndex][Polygons[PolygonIndex][0]]);
			OutRawMesh.WedgeIndices.Add(Faces[FaceIndex][Polygons[PolygonIndex][1]]);
			OutRawMesh.WedgeIndices.Add(Faces[FaceIndex][Polygons[PolygonIndex][2]]);
			OutRawMesh.WedgeTexCoords[0].Add(TextureCoord[PolygonIndex][0]);
			OutRawMesh.WedgeTexCoords[0].Add(TextureCoord[PolygonIndex][1]);
			OutRawMesh.WedgeTexCoords[0].Add(TextureCoord[PolygonIndex][2]);
			OutRawMesh.FaceMaterialIndices.Add(0);
			OutRawMesh.FaceSmoothingMasks.Add(0);
		}
	}
	return OutRawMesh.IsValidOrFixable();
}