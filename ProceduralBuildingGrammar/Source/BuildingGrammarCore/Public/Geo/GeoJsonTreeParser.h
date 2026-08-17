#pragma once

#include "CoreMinimal.h"
#include "Trees/TreeTypes.h"

// Parses a GeoJSON FeatureCollection of Point features into tree points -- purpose-built for
// German municipal tree-cadastre ("Baumkataster") exports like Karlsruhe's own (the
// baumart_allgemein property ParseTreeType reads is that dataset's own schema; other GeoJSON point
// data parses geometry fine but always resolves EGrammarTreeType::Unknown, since it won't have that
// property). Not a general-purpose GeoJSON library -- only top-level "features" with
// geometry.type=="Point" are read; other geometry types (LineString/Polygon/...) and any feature
// missing usable coordinates are skipped rather than treated as errors, matching
// FOsmDocument::ParseFile's own "skip what's unrecognized" contract.
class BUILDINGGRAMMARCORE_API FGeoJsonTreeParser
{
public:
	// Returns false and fills OutError on a malformed/unreadable file, or one that isn't a
	// FeatureCollection at all (no "features" array). OutTrees is reset first either way.
	static bool ParseFile(const FString& FilePath, TArray<FGrammarTreePoint>& OutTrees, FString& OutError);

	// Maps the Baumkataster schema's baumart_allgemein value ("Obstbaum"/"Laubbaum"/anything else,
	// including empty) onto EGrammarTreeType. Case-insensitive (real exports are consistent, but
	// this costs nothing to be lenient about). Exposed separately from ParseFile so it's
	// independently testable/reusable.
	static EGrammarTreeType ParseTreeType(const FString& BaumartAllgemein);
};
