#pragma once

#include "CoreMinimal.h"
#include "Config/RoofStyleConfig.h"
#include "Config/FacadeStyleConfig.h"
#include "Spec/PlacementRecord.h"

// Port of grammar.py's roof-mounted detail generators (_roof_detail_meshes and everything it
// aggregates: tiles/roof windows/dormers/chimneys; plus the separately-called _gutter_meshes,
// _roof_edge_meshes, _roof_service_meshes, _antenna_meshes/_antenna_instance_meshes). All become
// FGrammarPlacementRecord -- see GrammarPlacementHelpers.h.
namespace GrammarRoofDetails
{
	TArray<FGrammarPlacementRecord> RoofDetailPlacements(const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags);

	TArray<FGrammarPlacementRecord> GutterPlacements(const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof);

	TArray<FGrammarPlacementRecord> RoofEdgePlacements(const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof);

	TArray<FGrammarPlacementRecord> RoofServicePlacements(const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof, const FFacadeStyleConfig& Style, const TMap<FString, FString>& Tags);

	TArray<FGrammarPlacementRecord> AntennaPlacements(const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof, const FFacadeStyleConfig& Style);
}
