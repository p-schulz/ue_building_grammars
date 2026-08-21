#pragma once

#include "CoreMinimal.h"

struct FGrammarBlockInput;
class UWorld;

// The only FlexNetwork-touching piece of the road-network-driven building generation feature --
// reads UFlexNetworkSubsystem's live road graph, tessellates each segment via its own cached
// ArcLengthTable (no new tessellation code needed), converts UE world centimeters to
// BuildingGrammarCore's meters working space, and derives a per-road inset distance
// (URoadTypeProfile::GetOuterExtent(), curb + sidewalk) and a coarse building=commercial/residential
// tag hint (URoadTypeProfile::RoadDominanceLevel) -- then hands off to
// FGrammarBlockExtraction::ExtractBlocks (BuildingGrammarCore, FlexNetwork-agnostic) for the actual
// face-tracing. Everything past this file (block extraction, parcel subdivision, building
// generation) has no FlexNetwork dependency at all.
class FFlexRoadBlockExtraction
{
public:
	// Returns one FGrammarBlockInput per traced block in World's current FlexNetwork road graph, in
	// BuildingGrammarCore's meters working space, ready for
	// UBuildingGenerationLibrary::GenerateBuildingsFromBlocks. Empty if World has no
	// UFlexNetworkSubsystem road graph (nothing has been drawn/imported yet).
	static TArray<FGrammarBlockInput> ExtractBlockInputsFromFlexNetwork(UWorld* World);
};
