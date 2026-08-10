#pragma once

#include "CoreMinimal.h"
#include "Config/FacadeStyleConfig.h"
#include "Spec/PlacementRecord.h"

// Port of grammar.py's _facade_depth_meshes (street-level retail/industrial/parking detail +
// stair cores) and _facade_pattern_meshes (panel seams / insulation bands / ornament bands +
// pilasters, all keyed by the same StyleTokens keyword-set mechanism as
// GrammarEngineInternal::StyleTokens). All become FGrammarPlacementRecord.
namespace GrammarFacadeDepth
{
	// Tokens must be GrammarEngineInternal::StyleTokens(Style, Tags) for the side's Style/Tags --
	// computed once by the caller (BuildingGrammarEngine.cpp's per-side loop) rather than per call,
	// since this and the classification checks it makes (retail/industrial/parking/stair-core) used
	// to each recompute StyleTokens from scratch. Tags is still needed alongside Tokens for
	// IsRetailStyle/IsIndustrialStyle's direct shop=*/industrial=* tag shortcuts.
	TArray<FGrammarPlacementRecord> FacadeDepthPlacements(int32 SideIndex, const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, int32 StreetSideIndex, const TArray<double>& FloorHeights, double TotalHeight, const TSet<FString>& Tokens, const TMap<FString, FString>& Tags);
}
