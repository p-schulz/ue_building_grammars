#pragma once

#include "CoreMinimal.h"
#include "Config/FacadeStyleConfig.h"

// Port of config.py's style-selection free functions: exact tag/building-value matching first,
// then a semantic-keyword scoring fallback so untagged/loosely-tagged buildings still land on a
// plausible style (e.g. an OSM start_date before 1918 nudges toward Gründerzeit/Jugendstil/
// Fachwerk styles by name/material keyword overlap).
class BUILDINGGRAMMARCORE_API FGrammarStyleSelection
{
public:
	static bool BuildingValueIsExcluded(const TMap<FString, FString>& Tags, const TArray<FString>& ExcludedBuildingValues, const TArray<FFacadeStyleConfig>& Styles);

	static TArray<const FFacadeStyleConfig*> MatchingStylesForTags(const TMap<FString, FString>& Tags, const TArray<FFacadeStyleConfig>& Styles);

	// Exact matches first; else semantic-keyword scoring; else styles with no matching criteria at
	// all (i.e. "catch-all" styles); else every style. Never empty if Styles is non-empty.
	static TArray<const FFacadeStyleConfig*> SelectableStylesForTags(const TMap<FString, FString>& Tags, const TArray<FFacadeStyleConfig>& Styles);

	static bool StyleMatchesTags(const FFacadeStyleConfig& Style, const TMap<FString, FString>& Tags);

	static TArray<const FFacadeStyleConfig*> SemanticStylesForTags(const TMap<FString, FString>& Tags, const TArray<FFacadeStyleConfig>& Styles);

	static TMap<FString, int32> SemanticStyleKeywords(const TMap<FString, FString>& Tags);

	// Extracts a plausible 4-digit year from an OSM start_date-shaped string ("1912", "1908-1910",
	// "circa 1920", ...) by taking the first run of digits and reading its first 4 characters.
	// Unset if no digit run found or the result isn't in [1000, 2200].
	static TOptional<int32> StartYear(const FString& Value);
};
