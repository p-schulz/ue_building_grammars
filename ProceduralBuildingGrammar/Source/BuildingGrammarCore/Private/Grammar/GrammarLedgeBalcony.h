#pragma once

#include "CoreMinimal.h"
#include "Config/FacadeStyleConfig.h"
#include "Spec/PlacementRecord.h"

// Port of grammar.py's _ledge_mesh, _balcony_mesh, _balcony_detail_meshes. All become
// FGrammarPlacementRecord -- see GrammarPlacementHelpers.h.
namespace GrammarLedgeBalcony
{
	// grammar.py renders a ledge as a single zero-thickness flat quad spanning the whole edge at a
	// fixed Z (LedgeStyleConfig has no thickness field). This port instead gives it a small nominal
	// thickness (see .cpp) so it becomes a real length-normalized box kit part like roof
	// edges/gutters, rather than a degenerate zero-height placement -- a v1 simplification, visually
	// near-identical, that also avoids a zero-scale-axis transform.
	FGrammarPlacementRecord LedgePlacement(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double FloorBottom, const FFacadeStyleConfig& Style);

	FGrammarPlacementRecord BalconyPlacement(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double FloorBottom, const FFacadeStyleConfig& Style);
	TArray<FGrammarPlacementRecord> BalconyDetailPlacements(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double FloorBottom, const FFacadeStyleConfig& Style);
}
