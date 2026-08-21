#pragma once

#include "CoreMinimal.h"
#include "Parcel/GrammarParcelTypes.h"

// Port of the user-supplied parcel_subdivision_algorithms.hpp reference implementation (itself a
// dependency-free stand-in for Vanegas et al.-style parcel subdivision -- OBB recursion, skeleton
// with/without an inner offset, and a hybrid of the two). Reuses FGrammarGeometry2D
// (Geometry/GrammarGeometry2D.h) for every primitive that already exists there (SignedPolygonArea,
// PointInRing, Distance2D, CleanFootprint, Centroid2D, Normalize2D) rather than reimplementing them;
// everything below is genuinely new (no existing equivalent), kept on this class rather than added
// to FGrammarGeometry2D to keep that class scoped to what it's always been (grammar.py's ported
// free functions) -- same reasoning as FGrammarRoofSkeleton being its own class.
//
// Determinism: FRandomStream (seeded from FGrammarParcelConfig::Seed) replaces the reference
// implementation's hand-rolled xorshift Rng -- UE's own idiomatic seeded PRNG, one less thing to
// port/maintain, same "same seed -> same result" contract.
class BUILDINGGRAMMARCORE_API FGrammarParcelSubdivision
{
public:
	// Single dispatching entry point -- BlockBoundary is a closed ring, meters, no repeated first
	// point (same convention as FBuildingFootprint::OuterRing once projected). Internally cleans/
	// orients nothing itself -- callers that got BlockBoundary from an unvalidated source should run
	// it through FGrammarGeometry2D::CleanFootprint first, the same expectation GenerateBuildingSpec
	// has of its own Footprint parameter.
	static FGrammarParcelSubdivisionResult Subdivide(
		const TArray<FVector2D>& BlockBoundary,
		const FGrammarParcelConfig& Config,
		EGrammarParcelSubdivisionMethod Method,
		int32 BlockId = 0);

	// ---- Generic polygon validation primitives (no existing equivalent in FGrammarGeometry2D) --
	// public because block extraction (GrammarBlockExtraction.h) also needs these to validate traced
	// block polygons, not just parcel subdivision internals.

	// Strict segment intersection (proper crossing only) -- deliberately excludes shared endpoints/
	// collinear touches, since adjacent polygon edges are expected to share vertices.
	static bool SegmentsIntersect(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D);

	// Rejects self-intersecting (bow-tie) and degenerate polygons. O(n^2), fine for parcel/block
	// vertex counts.
	static bool IsSimplePolygon(const TArray<FVector2D>& Polygon, double MinArea = 1e-6);

	// A point guaranteed to be inside a simple polygon, convex or concave. FGrammarGeometry2D::
	// Centroid2D's area-weighted centroid can land outside a concave polygon (e.g. in the notch of
	// an L-shaped block) -- this tries that first, then falls back to a multi-scanline
	// widest-interior-span search, robust for any non-degenerate simple polygon.
	static FVector2D InteriorAnchor(const TArray<FVector2D>& Polygon);

private:
	static FGrammarParcelSubdivisionResult SubdivideObb(const TArray<FVector2D>& BlockBoundary, const FGrammarParcelConfig& Config, int32 BlockId);
	static FGrammarParcelSubdivisionResult SubdivideSkeletonNoOffset(const TArray<FVector2D>& BlockBoundary, const FGrammarParcelConfig& Config, int32 BlockId);
	static FGrammarParcelSubdivisionResult SubdivideSkeletonWithOffset(const TArray<FVector2D>& BlockBoundary, const FGrammarParcelConfig& Config, bool bEmitInnerPatio, int32 BlockId);
	static FGrammarParcelSubdivisionResult SubdivideHybrid(const TArray<FVector2D>& BlockBoundary, const FGrammarParcelConfig& Config, int32 BlockId);
};
