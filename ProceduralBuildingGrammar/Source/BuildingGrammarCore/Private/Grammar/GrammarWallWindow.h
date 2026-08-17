#pragma once

#include "CoreMinimal.h"
#include "Config/FacadeStyleConfig.h"
#include "Spec/MeshSpec.h"
#include "Spec/PlacementRecord.h"
#include "Geometry/GrammarWallRecess.h"

// Port of grammar.py's window_offsets, _wall_mesh, _wall_row_mesh, _window_mesh,
// _window_detail_meshes. Wall meshes are hero geometry (FGrammarMeshSpec); the window pane and
// every window detail (frame/mullion/sill/shutter) become FGrammarPlacementRecord -- see
// GrammarPlacementHelpers.h for why.
namespace GrammarWallWindow
{
	// Evenly spaces as many windows as fit Length minus 2*Margin at >= Spacing apart, centered.
	// Empty if even one window's Width doesn't fit within the margins.
	TArray<double> WindowOffsets(double Length, double Width, double Spacing, double Margin);

	// Exposed so BuildingGrammarEngine.cpp's per-side loop can gather each window's FGrammarWallOpening
	// rectangle (for FGrammarWallRecess::BuildSegments) using the exact same vertical-extent math
	// WindowPlacement/WindowDetailPlacements already use internally.
	void WindowVerticalExtent(const FWindowStyleConfig& Window, double FloorHeight, double& OutHeight, double& OutBottomOffset);

	// Openings already resolved by the caller (BuildingGrammarEngine.cpp) -- see that file's per-side
	// loop and FGrammarWallRecess::BuildSegments for how the wall rectangle gets notched around them.
	// Normal must be this same side's own outward normal (FGrammarGeometry2D::OutwardNormal against
	// the footprint's actual winding) -- these functions can't safely recompute it themselves since
	// that depends on the whole footprint, not just this one edge.
	FGrammarMeshSpec WallMesh(const FString& MeshSourceName, int32 SideIndex, const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Height, const FFacadeStyleConfig& Style, const FString& SourceName, const TArray<FGrammarWallOpening>& Openings);
	FGrammarMeshSpec WallRowMesh(const FString& SourceName, int32 SideIndex, int32 FloorIndex, const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double FloorBottom, double FloorHeight, const FFacadeStyleConfig& Style, const TArray<FGrammarWallOpening>& Openings);

	FGrammarPlacementRecord WindowPlacement(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double FloorBottom, double FloorHeight, const FFacadeStyleConfig& Style);

	// bHasShutters must be GrammarEngineInternal::StyleHasShutters(GrammarEngineInternal::StyleTokens(Style, {}))
	// -- computed once per facade side by the caller (BuildingGrammarEngine.cpp's per-side loop)
	// rather than recomputed on every call, since this is invoked once per window per floor per
	// side.
	TArray<FGrammarPlacementRecord> WindowDetailPlacements(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double FloorBottom, double FloorHeight, const FFacadeStyleConfig& Style, bool bHasShutters);
}
