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

	// Parses an OSM XML (.osm) file via Unreal's XmlParser module (FXmlFile). Returns false and
	// fills OutError on a malformed/unreadable file; unrecognized elements are skipped rather than
	// treated as errors.
	static bool ParseFile(const FString& FilePath, FOsmDocument& OutDocument, FString& OutError);

	// Center of this document's node bounding box -- a reasonable default projection origin
	// (FLocalTangentPlaneProjection) when the caller has no more specific point in mind, e.g. an
	// editor tool that just wants "somewhere sensible" for a freshly opened .osm file. False if
	// Nodes is empty.
	bool ComputeBoundsCenter(double& OutCenterLatitude, double& OutCenterLongitude) const;
};
