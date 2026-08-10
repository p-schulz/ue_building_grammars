#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"

// Shared 2D polygon/edge primitives used throughout the grammar engine. Building footprints and
// facade-edge math are worked out in the horizontal plane (FVector2D) with height handled
// separately by the caller, mirroring procedural_building_grammar's grammar.py (Point2D = (x, y),
// height applied only when a vertex is finally lifted to 3D).
//
// Ported 1:1 from grammar.py's free functions (clean_footprint, orient_footprint_ccw,
// outward_normal, _tangent, _point_on_segment, _move, _offset_point, _segments, _bounds,
// _centroid_2d, _normalize_2d, distance_2d) plus point-in-polygon / point-to-segment distance used
// by OSM building-part parent matching (blender_adapter.py's _matching_parent).
class BUILDINGGRAMMARCORE_API FGrammarGeometry2D
{
public:
	// One edge of a closed ring, Start -> End.
	struct FEdge
	{
		FVector2D Start = FVector2D::ZeroVector;
		FVector2D End = FVector2D::ZeroVector;
	};

	// Removes consecutive duplicate points (by XY) and drops a final point that duplicates the
	// first (closes-the-loop redundancy), matching clean_footprint's dedupe behavior.
	static TArray<FVector2D> CleanFootprint(const TArray<FVector2D>& Footprint);

	// Shoelace formula; positive = CCW, matching signed_polygon_area (footprint is cleaned first).
	static double SignedPolygonArea(const TArray<FVector2D>& Footprint);

	static bool PolygonIsCCW(const TArray<FVector2D>& Footprint);

	// Cleans, then reverses winding if the result is not already CCW.
	static TArray<FVector2D> OrientFootprintCCW(const TArray<FVector2D>& Footprint);

	// Closed-loop consecutive edge pairs: (P0,P1), (P1,P2), ..., (Pn-1,P0).
	static TArray<FEdge> GetSegments(const TArray<FVector2D>& Ring);

	// Outward-facing normal of the Start->End edge, given the ring's winding.
	static FVector2D OutwardNormal(const FVector2D& Start, const FVector2D& End, bool bCCW);

	// Unit vector from Start to End; zero vector if Start == End.
	static FVector2D Tangent(const FVector2D& Start, const FVector2D& End);

	// Start + Tangent * OffsetAlongEdge + Normal * DepthOutward -- the standard "place a point on
	// a wall segment, offset along it, pushed out/in by depth" helper used by nearly every detail
	// mesh (windows, doors, ledges, balconies, roof trim, ...).
	static FVector2D PointOnSegment(const FVector2D& Start, const FVector2D& Tangent, const FVector2D& Normal, double OffsetAlongEdge, double DepthOutward);

	static FVector2D Move(const FVector2D& Point, const FVector2D& Direction, double Distance);

	static FVector2D OffsetPoint(const FVector2D& Point, const FVector2D& Normal, double Distance);

	static double Distance2D(const FVector2D& A, const FVector2D& B);

	static FVector2D Centroid2D(const TArray<FVector2D>& Points);

	// Returns ZeroVector if the input is (near) zero-length, matching _normalize_2d's safe-guard.
	static FVector2D Normalize2D(const FVector2D& V);

	// (MinX, MinY, MaxX, MaxY).
	static FBox2D Bounds(const TArray<FVector2D>& Points);

	// Unit direction of Points' own longest boundary edge (Points treated as a closed ring -- see
	// GetSegments), used as the default ridge/detail alignment for gabled/hipped roofs and their
	// tiles/dormers/chimneys when no OSM orientation tag is present. Deliberately NOT the wider axis
	// of the axis-aligned bounding box -- that would snap every rotated footprint's roof features to
	// world North/East instead of following the building's own shape.
	static FVector2D LongestAxisDirection(const TArray<FVector2D>& Points);

	static double PointToSegmentDistanceSquared(const FVector2D& P, const FVector2D& A, const FVector2D& B);

	static double PointToPolylineDistance(const FVector2D& P, const TArray<FVector2D>& Polyline);

	// Standard even-odd ray-casting test; boundary points count as inside.
	static bool PointInRing(const FVector2D& P, const TArray<FVector2D>& Ring);

	// Cumulative prefix sum of floor heights -- floor 0 starts at Z=0 (_floor_bottoms).
	static TArray<double> FloorBottoms(const TArray<double>& FloorHeights);
};
