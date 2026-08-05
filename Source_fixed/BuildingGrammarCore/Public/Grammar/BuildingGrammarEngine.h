#pragma once

#include "CoreMinimal.h"
#include "Config/BuildingGrammarConfig.h"
#include "Spec/BuildingSpec.h"

// Port of grammar.py's generate_building_spec -- the single public entry point of the grammar
// engine (see __init__.py's __all__ list, the closest thing the Python add-on has to a declared
// public API surface). Everything else (style selection, level inference, wall/window/door/ledge/
// balcony/roof/roof-detail/facade-depth generation) is module-private implementation living under
// BuildingGrammarCore's Private/Grammar/ folder.
class BUILDINGGRAMMARCORE_API FBuildingGrammarEngine
{
public:
	// Footprint is a closed 2D ring, already projected to a metric space (e.g. UE centimeters --
	// see FLocalTangentPlaneProjection); winding/duplicate-vertex cleanup happens internally.
	//
	// Returns false (with OutError set, OutSpec untouched) if the footprint has fewer than 3
	// distinct points after cleanup, or if the building's `building=*` value is in
	// Config.ExcludedBuildingValues and no style explicitly matches it -- grammar.py raises
	// ValueError for both cases; this port uses a return-value/OutError convention instead of C++
	// exceptions, matching typical Unreal code style for expected per-building skip conditions.
	static bool GenerateBuildingSpec(const TArray<FVector2D>& Footprint, const TMap<FString, FString>& Tags, const FBuildingGrammarConfig& Config, const FString& SourceName, FGrammarBuildingSpec& OutSpec, FString& OutError);
};
