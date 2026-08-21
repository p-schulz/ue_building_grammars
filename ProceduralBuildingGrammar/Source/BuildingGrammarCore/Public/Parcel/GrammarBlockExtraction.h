#pragma once

#include "CoreMinimal.h"
#include "GrammarBlockExtraction.generated.h"

// One road's centerline, already tessellated into a polyline (meters), plus enough per-road
// information to inset the block boundary away from it. Deliberately engine/FlexNetwork-agnostic --
// the caller (e.g. an Editor-only FlexNetwork adapter) does the tessellation and unit conversion;
// this struct and FGrammarBlockExtraction below only ever see plain 2D geometry, so they're
// unit-testable without a UWorld and reusable by any future road source.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarRoadPolyline
{
	GENERATED_BODY()

	// Meters. At least 2 points; Points[0]/Points.Last() are this road's two end nodes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	TArray<FVector2D> Points;

	// Meters, symmetric both sides -- how far the block boundary is inset from this road's
	// centerline (e.g. URoadTypeProfile::GetOuterExtent(), curb + sidewalk, converted to meters).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	double InsetDistance = 0.0;

	// Caller-defined opaque tag carried through into FGrammarBlock::BoundingRoadIds -- not
	// interpreted by this class at all. The FlexNetwork adapter uses this to smuggle a serialized
	// RoadDominanceLevel through so a caller can derive a building=commercial/residential tag hint
	// from which roads bound a given block.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	FString RoadId;
};

// One traced, inset block boundary.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarBlock
{
	GENERATED_BODY()

	// Meters, closed ring, no repeated first point, CCW-wound (matches FGrammarGeometry2D's own
	// convention and is guaranteed by construction -- see ExtractBlocks's sign-convention comment).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block")
	TArray<FVector2D> Polygon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block")
	double AreaM2 = 0.0;

	// RoadId of every road segment that contributed at least one edge to this block's boundary, in
	// no particular order, duplicates possible (a long road contributing multiple tessellated
	// segments to the same block appears once per segment).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block")
	TArray<FString> BoundingRoadIds;
};

// Traces the closed regions ("blocks") a set of road polylines encloses -- the same problem
// Plugins/ProceduralRoads/Source/ProceduralRoadsPCG/Private/BuildingBlockDetection.cpp solves
// against a different road data model (osm2xodr::model::MapModel, not FlexNetwork). Same standard
// DCEL/planar-subdivision face-tracing rule ("next half-edge clockwise from the twin of the one just
// traversed" -- see ExtractBlocks's own comment for why this direction, not counterclockwise,
// matters at any real intersection), substantially simplified here because a real road-graph node
// (one canonical point per intersection, no connector-fillet segments to skip) doesn't need that
// file's CanonicalNode/compound-junction-clustering machinery. Also splits roads at points where
// they geometrically cross a different road without already sharing a node there (SplitRoadsAt-
// Intersections, in the .cpp) -- the common FlexNetwork case of two roads drawn crossing in the
// viewport with no shared FFlexRoadNode -- before any of the above runs.
class BUILDINGGRAMMARCORE_API FGrammarBlockExtraction
{
public:
	static TArray<FGrammarBlock> ExtractBlocks(const TArray<FGrammarRoadPolyline>& Roads);
};
