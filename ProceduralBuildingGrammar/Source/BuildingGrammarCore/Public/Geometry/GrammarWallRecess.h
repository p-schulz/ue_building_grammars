#pragma once

#include "CoreMinimal.h"

// One window/door opening on a wall edge, in the edge's own local frame: S is offset along the edge
// (meters, same frame as FGrammarGeometry2D::PointOnSegment's OffsetAlongEdge / GrammarWallWindow::
// WindowOffsets), Z is world height (meters). RecessDepth<=0 means this opening stays flush -- it's
// left out of the wall's cut list entirely, matching this codebase's "zero disables" convention (e.g.
// FRoofStyleConfig::ChimneyCount).
struct BUILDINGGRAMMARCORE_API FGrammarWallOpening
{
	double SLeft = 0.0;
	double SRight = 0.0;
	double ZBottom = 0.0;
	double ZTop = 0.0;
	double RecessDepth = 0.0;
};

// One quad's 4 corners, world-space (meters, BuildingGrammarCore's convention -- see FGrammarMeshSpec's
// own comment), already in the correct CCW order for ITS OWN visible face -- callers never need a
// separate stored normal, matching how every mesh builder in this codebase (FGrammarDynamicMeshBuilder,
// PCGExtrudeFootprintToWalls.cpp's AppendTriangleWithComputedNormal) already derives face normals
// purely from vertex winding.
struct BUILDINGGRAMMARCORE_API FGrammarWallQuad
{
	FVector Corners[4];
};

// Cuts recessed window/door pockets into an otherwise-flat wall rectangle -- the shared geometry both
// the classic engine (GrammarWallWindow.cpp) and the PCG pipeline (PCGExtrudeFootprintToWalls.cpp)
// build on, so a "wall reveal" only has to be derived once. See those two callers' own comments for
// how each turns the returned quads into its own mesh representation.
namespace FGrammarWallRecess
{
	// Decomposes the wall rectangle spanned by [Start,End] horizontally (in the Start->End direction)
	// and [WallBottom,WallTop] vertically into a list of quads: the untouched flush remainder (same
	// plane as an ordinary flat wall, Normal-offset 0) plus, for each Opening that both has a positive
	// RecessDepth and fits within the rectangle with real margin on every side, a recessed pocket -- a
	// back quad pushed inward by RecessDepth (Normal-offset -RecessDepth) plus four reveal quads
	// (left/right jambs, sill, head) bridging it back out to the flush plane. Openings failing either
	// condition are left out of the cut entirely (silently skipped, not an error) -- the wall renders
	// flush there exactly as it did before this feature existed.
	//
	// Multiple Openings sharing the same [SLeft,SRight] range (e.g. the same window column repeated
	// on several floors, since WindowOffsets is computed once per side and reused per floor) are fully
	// supported: each becomes its own independently recessed pocket, stacked vertically with flush
	// bands filling the gaps between them and at the top/bottom of the wall.
	//
	// Winding for the jamb/sill/head reveal quads is derived from the same Tangent x OutwardNormal =
	// -Up identity already verified elsewhere in this codebase (see PCGFacadeWindowDoorLayout.cpp's
	// AddPlacement comment) rather than guessed -- see the .cpp for the derivation.
	BUILDINGGRAMMARCORE_API TArray<FGrammarWallQuad> BuildSegments(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double WallBottom, double WallTop, TArray<FGrammarWallOpening> Openings);
}
