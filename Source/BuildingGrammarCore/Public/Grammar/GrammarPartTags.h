#pragma once

#include "CoreMinimal.h"
#include "Config/BuildingGrammarConfig.h"

// Ports grammar.py's effective_part_min_height / tags_for_building_part_volume: given an OSM
// building:part's tags, works out its vertical offset from the ground (its "min height") and
// rewrites height/level tags so the grammar engine generates only the part's own slice, not the
// full height from the ground up.
//
// Minor documented divergence from the Python source: grammar.py's `tags.get("A") or
// tags.get("B")` fallback only tries B when A is *missing or empty*, not when A is present but
// fails to parse as a number -- so a bogus `min_height` tag alongside a valid `building:min_height`
// would (in Python) leave the result unset rather than falling back. This port instead falls back
// to B whenever A fails to parse for any reason, which is strictly more robust and differs only in
// this deliberately-unlikely edge case (a tag present but garbage).
class BUILDINGGRAMMARCORE_API FGrammarPartTags
{
public:
	static double EffectivePartMinHeight(const TMap<FString, FString>& Tags, const FBuildingGrammarConfig& Config);

	// Returns (rewritten tags, min height). If min height is 0, returns Tags unchanged.
	static TPair<TMap<FString, FString>, double> TagsForBuildingPartVolume(const TMap<FString, FString>& Tags, const FBuildingGrammarConfig& Config);
};
