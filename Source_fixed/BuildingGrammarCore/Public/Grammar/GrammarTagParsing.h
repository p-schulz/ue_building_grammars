#pragma once

#include "CoreMinimal.h"

// Shared OSM tag-value parsing, used by building-part resolution and (later) level/floor-height
// inference. Ports grammar.py's parse_meters and _parse_int.
class BUILDINGGRAMMARCORE_API FGrammarTagParsing
{
public:
	// Parses "<number>", "<number>m", "<number> meters", or "<number> metres" (case-insensitive).
	// Unset if Value is null/empty/unparseable.
	static TOptional<double> ParseMeters(const FString* Value);

	// int(float(value)) semantics (accepts "4" and "4.0" alike, truncates toward zero). Unset if
	// Value is null/empty/unparseable.
	static TOptional<int32> ParseIntTag(const FString* Value);
};
