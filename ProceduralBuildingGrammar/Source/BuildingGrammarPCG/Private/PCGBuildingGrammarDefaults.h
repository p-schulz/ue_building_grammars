#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "Metadata/PCGMetadata.h"

struct FBuildingGrammarConfig;

// Loads the plugin's bundled Content/german_building_grammar_config.json -- the same file the
// classic (non-PCG) "Load Preset Config from JSON..." Tools-menu action loads on demand -- as the
// default config new PCG data-source/style nodes start with, so a user doesn't have to manually
// populate Config/Styles by hand or go through that menu action first before wiring up a PCG graph.
// Not module-exported (BUILDINGGRAMMARPCG_API): only used within this same module's own
// translation units. Returns false (OutConfig left untouched) if the plugin or the file can't be
// found/parsed -- callers should tolerate this (nodes must still construct successfully with a
// plain default config if the bundled file is ever missing/moved).
bool LoadDefaultGermanBuildingGrammarConfig(FBuildingGrammarConfig& OutConfig);

// Shared by every node that needs to correlate its own per-building input data (Footprint/Edges/
// Walls, all tagged "SourceName:<value>" by UPCGLoadOsmBuildingVolumesSettings and propagated
// downstream unchanged) with a StyleInfo row keyed by the same SourceName (UPCGSelectFacadeStyleSettings'
// UPCGParamData output, looked up via FindMetadataKey(FName(SourceName))). Returns an empty string
// (no match) if Tags carries no "SourceName:" entry.
FString ExtractSourceNameFromTags(const TSet<FString>& Tags);

// Shared by every node that needs a BuildingInfo/StyleInfo row's TagsJson column (a serialized JSON
// object of every raw OSM tag) turned back into a plain tag map.
TMap<FString, FString> DeserializeTagsFromJson(const FString& Json);

// One line segment of a street way, already in world-space UE centimeters (see
// UPCGGetStreetNetworkSettings' header comment for its own coordinate convention -- matches this
// pipeline's as long as the same OSM file/origin feeds both nodes). Name is the street's `name` tag
// if it had one (empty otherwise), carried through so addr:street matching can prefer it the same
// way FGrammarStreetAlignment::ApplyRidgeDirectionTags does (BuildingGrammarCore/Osm/
// StreetRidgeAlignment.cpp) -- not reused directly (that class only ever produces a ridge direction,
// not an edge index, and its nearest-segment helper is private to that .cpp), but the same priority
// structure (name match beats raw proximity; raw proximity only within a search radius) is
// deliberately mirrored by DetermineStreetFacingSideIndex below.
struct FStreetSegment
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	FString Name;
};

// UPCGGetStreetNetworkSettings tags each Streets entry "Name:<value>" (empty for unnamed ways) --
// same prefix-tag convention as ExtractSourceNameFromTags above, just a different prefix.
FString ExtractStreetNameFromTags(const TSet<FString>& Tags);

// Port of GrammarEngineInternal::StreetFacingSideIndex's explicit-tag tiers (a faithful port of a
// stub -- these OSM tags are never actually written anywhere in this codebase today, so classic's own
// street-facing detection is already this same stub, not real street-network proximity matching on
// its own), extended with two more tiers that DO use real street geometry when StreetSegments is
// non-empty -- addr:street name match (regardless of SearchRadius, an explicit address being a
// stronger signal than raw proximity -- same reasoning FGrammarStreetAlignment::
// ApplyRidgeDirectionTags uses), else nearest edge to the nearest street among all of them, only
// within SearchRadius. Priority, highest first: explicit grammar:street:point/
// grammar:street_facing_side tag (an explicit per-building override) -> addr:street name match
// against real street geometry -> nearest real street within SearchRadius -> edge 0. Tag coordinates
// are in BuildingGrammarCore's meters working unit (the space the footprint was in before this
// pipeline's meters->cm conversion), converted to cm here to compare against EdgePoints' already-cm
// Transform locations. Shared by every node that needs to know which footprint edge faces the street
// (UPCGFacadeWindowDoorLayoutSettings' door placement, UPCGFacadePatternStreetDetailLayoutSettings'
// street-level detail, UPCGRoofFrameGeneratorSettings' ClosestStreet ridge alignment).
int32 DetermineStreetFacingSideIndex(const TMap<FString, FString>& Tags, const TArray<FPCGPoint>& EdgePoints, const FPCGMetadataAttribute<double>* LengthAttr, const TArray<FStreetSegment>& StreetSegments, double SearchRadius);

// Nearest point on any of Candidates to Footprint's centroid (2D, X/Y only) -- used by
// UPCGRoofFrameGeneratorSettings' ClosestStreet ridge-direction tier (a different query than
// DetermineStreetFacingSideIndex's nearest-EDGE query above, over the same segment list), mirroring
// FGrammarStreetAlignment::ApplyRidgeDirectionTags' own centroid-based nearest-segment search
// (BuildingGrammarCore/Osm/StreetRidgeAlignment.cpp) -- returns the segment's own local tangent
// (End-Start, normalized), matching that function's own "locally-correct even for a curving street"
// reasoning. Returns false (OutDirection untouched) if Candidates is empty or the nearest match is
// outside SearchRadius (ignored entirely for an addr:street-matched candidate list, same
// no-radius-limit reasoning as DetermineStreetFacingSideIndex's own addr:street tier -- pass a
// negative SearchRadius to disable the check).
bool FindNearestStreetDirection(const TArray<FVector2D>& Footprint, const TArray<const FStreetSegment*>& Candidates, double SearchRadius, FVector2D& OutDirection);
