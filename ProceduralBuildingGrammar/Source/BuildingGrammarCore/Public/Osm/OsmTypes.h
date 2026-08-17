#pragma once

#include "CoreMinimal.h"

// Raw OSM XML document model (nodes/ways/relations + tags), independent of any building-specific
// interpretation -- FBuildingFootprintAssembler (Osm/BuildingFootprintAssembler.h) turns this into
// FBuildingFootprint. Plain C++ structs (not USTRUCT/reflected): this is an internal parse
// intermediate, never exposed to Blueprint or serialized to a DataAsset.
struct BUILDINGGRAMMARCORE_API FOsmNode
{
	int64 Id = 0;
	double Lat = 0.0;
	double Lon = 0.0;

	// A plain OSM <node> is very commonly a standalone tagged feature in its own right (a street
	// lamp, a bench, a waste bin, ...), not just a vertex referenced by some way's NodeRefs -- see
	// ParseTagChildren's identical use for FOsmWay/FOsmRelation.
	TMap<FString, FString> Tags;
};

struct BUILDINGGRAMMARCORE_API FOsmWay
{
	int64 Id = 0;
	TArray<int64> NodeRefs;
	TMap<FString, FString> Tags;
};

struct BUILDINGGRAMMARCORE_API FOsmRelationMember
{
	FString Type;  // "node" | "way" | "relation"
	int64 Ref = 0;
	FString Role;  // "outer" | "inner" | "" | ...
};

struct BUILDINGGRAMMARCORE_API FOsmRelation
{
	int64 Id = 0;
	TArray<FOsmRelationMember> Members;
	TMap<FString, FString> Tags;
};

struct BUILDINGGRAMMARCORE_API FOsmDocument
{
	TMap<int64, FOsmNode> Nodes;
	TMap<int64, FOsmWay> Ways;
	TMap<int64, FOsmRelation> Relations;

	// True if this document's own <bounds minlat=".." maxlat=".." minlon=".." maxlon=".."/> element
	// was present and parsed -- the ORIGINAL query area, which may be larger than what GetBounds
	// would otherwise compute from just the nodes actually present. See GetBounds.
	bool bHasExplicitBounds = false;
	double BoundsMinLat = 0.0;
	double BoundsMaxLat = 0.0;
	double BoundsMinLon = 0.0;
	double BoundsMaxLon = 0.0;

	// Parses an OSM XML (.osm) file via Unreal's XmlParser module (FXmlFile). Returns false and
	// fills OutError on a malformed/unreadable file; unrecognized elements are skipped rather than
	// treated as errors.
	static bool ParseFile(const FString& FilePath, FOsmDocument& OutDocument, FString& OutError);

	// This document's own lat/lon extent -- the parsed <bounds> element if present, else computed
	// from every node's own Lat/Lon (a node-extent fallback still correctly covers everything the
	// file actually contains, just not necessarily the full area the file was originally queried
	// for). Returns false (Out params left untouched) only if there's neither an explicit <bounds>
	// element nor any nodes at all.
	bool GetBounds(double& OutMinLat, double& OutMaxLat, double& OutMinLon, double& OutMaxLon) const;

	// Midpoint of GetBounds() -- (MinLat+MaxLat)/2, (MinLon+MaxLon)/2 -- the standard shared
	// projection origin this project's various "generate/import" actions use so buildings, trees,
	// roads, and satellite imagery all land in the same local-tangent-plane space (see
	// FLocalTangentPlaneProjection). Same simple min/max-average convention
	// FBuildingFootprintAssembler::ComputeFootprintBoundsCenter used (not an area-weighted
	// centroid), but over the FILE's own bounds rather than just its building footprints -- doesn't
	// need footprints assembled at all, and still gives a usable origin for a file with no
	// buildings (a road-only or tree-only extract). Returns false (Out params left untouched) only
	// if GetBounds() itself would.
	bool GetBoundsCenter(double& OutCenterLatitude, double& OutCenterLongitude) const;
};
