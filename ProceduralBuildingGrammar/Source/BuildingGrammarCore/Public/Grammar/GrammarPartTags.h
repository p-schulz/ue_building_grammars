#pragma once

#include "CoreMinimal.h"
#include "Config/BuildingGrammarConfig.h"

// Ports grammar.py's effective_part_min_height / tags_for_building_part_volume: given an OSM
// building (or building:part)'s tags, works out its vertical offset from the ground (its
// "min height") and rewrites height/level tags so the grammar engine generates only that slice,
// not the full height from the ground up. Despite the name, FBuildingPartResolver::Resolve() calls
// this for every volume, not just building:part children -- a standalone building can carry its
// own explicit min_height tag too (e.g. one sitting above a passage/arcade); it's a no-op
// (Tags unchanged, 0.0) whenever none of min_height/building:min_height/min_level are present.
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
