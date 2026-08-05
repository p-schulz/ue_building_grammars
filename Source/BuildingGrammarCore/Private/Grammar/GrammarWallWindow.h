#pragma once

#include "CoreMinimal.h"
#include "Config/FacadeStyleConfig.h"
#include "Spec/MeshSpec.h"
#include "Spec/PlacementRecord.h"

// Port of grammar.py's window_offsets, _wall_mesh, _wall_row_mesh, _window_mesh,
// _window_detail_meshes. Wall meshes are hero geometry (FGrammarMeshSpec); the window pane and
// every window detail (frame/mullion/sill/shutter) become FGrammarPlacementRecord -- see
// GrammarPlacementHelpers.h for why.
namespace GrammarWallWindow
{
	// Evenly spaces as many windows as fit Length minus 2*Margin at >= Spacing apart, centered.
	// Empty if even one window's Width doesn't fit within the margins.
	TArray<double> WindowOffsets(double Length, double Width, double Spacing, double Margin);

	FGrammarMeshSpec WallMesh(const FString& MeshSourceName, int32 SideIndex, const FVector2D& Start, const FVector2D& End, double Height, const FFacadeStyleConfig& Style, const FString& SourceName);
	FGrammarMeshSpec WallRowMesh(const FString& SourceName, int32 SideIndex, int32 FloorIndex, const FVector2D& Start, const FVector2D& End, double FloorBottom, double FloorHeight, const FFacadeStyleConfig& Style);

	FGrammarPlacementRecord WindowPlacement(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double FloorBottom, double FloorHeight, const FFacadeStyleConfig& Style);
	TArray<FGrammarPlacementRecord> WindowDetailPlacements(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double FloorBottom, double FloorHeight, const FFacadeStyleConfig& Style);
}
