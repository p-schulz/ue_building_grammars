#pragma once

#include "CoreMinimal.h"
#include "Config/BuildingGrammarConfig.h"

// Port of grammar.py's infer_levels / floor_height_sequence.
class BUILDINGGRAMMARCORE_API FGrammarLevels
{
public:
	// Style may be null (matches Python's Optional[style] = None). Priority: explicit
	// building:levels/levels tag; else height tag / floor height, rounded; else the style's own
	// default level count; else the config's default level count. Always >= 1.
	static int32 InferLevels(const TMap<FString, FString>& Tags, const FBuildingGrammarConfig& Config, const FFacadeStyleConfig* Style);

	// One height per floor (index 0 = ground floor), using config-level irregular-floor-height
	// overrides, then uniformly rescaled to match an explicit OSM height tag if present.
	static TArray<double> FloorHeightSequence(int32 Levels, const TMap<FString, FString>& Tags, const FBuildingGrammarConfig& Config, const FFacadeStyleConfig* Style);
};
