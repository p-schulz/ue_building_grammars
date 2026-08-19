#pragma once

#include "CoreMinimal.h"
#include "Config/FacadeStyleConfig.h"
#include "Config/BuildingGrammarConfig.h"

// Shared helpers used across the grammar engine's per-category generation files (wall/window,
// door, ledge/balcony, facade-depth/pattern, roof, roof-detail). Module-private: not part of
// BuildingGrammarCore's public API, mirrors grammar.py's leading-underscore free functions.
namespace GrammarEngineInternal
{
	// Lowercased, punctuation-normalized bag of words drawn from the style's name/materials and a
	// fixed set of OSM tags -- the sole mechanism by which OSM tags steer optional facade/roof
	// detail (retail signage, industrial doors, shutters, stair cores, roof service clutter):
	// simple keyword matching, not a real OSM tag ontology. Port of grammar.py's _style_tokens.
	TSet<FString> StyleTokens(const FFacadeStyleConfig& Style, const TMap<FString, FString>& Tags);

	bool HasAny(const TSet<FString>& Tokens, const TSet<FString>& Values);

	// These all take an already-computed StyleTokens() result rather than (Style, Tags) so that a
	// caller iterating many facade sides/floors/windows for the same building can compute the
	// token set once (per Style -- see BuildingGrammarEngine.cpp's per-side loop) instead of
	// StyleTokens' string-join/lowercase/tokenize work being repeated for every call. Tokens must
	// be StyleTokens(Style, Tags) for IsRetailStyle/IsIndustrialStyle/IsParkingStyle/
	// ShouldAddStairCore, but StyleTokens(Style, {}) (no Tags) for StyleHasShutters -- deliberately
	// a different token set, so it isn't tag-triggered by unrelated OSM tag text; see
	// BuildingGrammarEngine.cpp for where both are computed.
	bool IsRetailStyle(const TSet<FString>& Tokens, const TMap<FString, FString>& Tags);
	bool IsIndustrialStyle(const TSet<FString>& Tokens, const TMap<FString, FString>& Tags);
	bool IsParkingStyle(const TSet<FString>& Tokens);
	bool StyleHasShutters(const TSet<FString>& StyleOnlyTokens);
	bool ShouldAddStairCore(const TSet<FString>& Tokens, bool bStreetFacing, double Length, double TotalHeight);

	// -1 index = no variant (always Style.WallColor); otherwise the chosen TArray index alongside
	// its color, used to build a distinct per-variant material name via WallMaterialName. Ports
	// _variant_wall_color/_row_wall_color/_wall_material_name.
	TPair<int32, FLinearColor> VariantWallColor(const FFacadeStyleConfig& Style, const FString& SourceName, int32 SideIndex);
	TPair<int32, FLinearColor> RowWallColor(const FFacadeStyleConfig& Style, int32 FloorIndex);
	FString WallMaterialName(const FString& Base, const FString& Kind, int32 ColorIndex);

	// Applies grammar:roof:* / roof:* OSM tag overrides on top of a base RoofStyleConfig (shape,
	// height, material, color). Port of grammar.py's _roof_from_tags. LevelHeight converts a
	// roof:levels fallback (a level COUNT, e.g. "2") into meters -- same DefaultFloorHeight
	// convention FGrammarLevels uses for building:levels, passed in by the caller since this
	// function otherwise has no access to Config/Style.
	FRoofStyleConfig RoofFromTags(const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags, double LevelHeight);

	// Applies facade:material/colour OSM tag overrides on top of a base FacadeStyleConfig -- an
	// explicit facade:colour tag also clears wall color variants (an explicit color always wins
	// outright rather than blending with the style's variant scheme). Port of
	// grammar.py's _facade_style_from_tags.
	FFacadeStyleConfig FacadeStyleFromTags(const FFacadeStyleConfig& Style, const TMap<FString, FString>& Tags);

	// "brick" -> "OSM Facade Brick" / "OSM Roof Brick" etc. Port of _semantic_material_name.
	FString SemanticMaterialName(const FString& Role, const FString& Value);

	// Parses an OSM colour value: "#rrggbb"/"#rgb" hex, or a small named-color table (falls back
	// to "grey"->"gray" normalization and ";"-separated multi-value tags taking the first value).
	// Unset if unrecognized. Port of _parse_osm_color.
	TOptional<FLinearColor> ParseOsmColor(const FString& Value);

	// Index of the footprint edge nearest the street: an explicit grammar:street:point tag wins,
	// else an explicit grammar:street_facing_side index tag, else edge 0. Port of
	// _street_facing_side_index/_street_reference_point_from_tags/_point_segment_distance_sq_2d.
	int32 StreetFacingSideIndex(const TArray<FVector2D>& Footprint, const TMap<FString, FString>& Tags);

	bool DoorApplies(const FFacadeStyleConfig& Style, int32 SideIndex, int32 StreetSideIndex, const TMap<FString, FString>& Tags);
	bool WindowOverlapsDoor(double Offset, double DoorOffset, const FFacadeStyleConfig& Style);
}
