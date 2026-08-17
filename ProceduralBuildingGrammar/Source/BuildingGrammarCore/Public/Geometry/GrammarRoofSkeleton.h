#pragma once

#include "CoreMinimal.h"

// Straight-skeleton-based hip-roof geometry for an arbitrary simple polygon footprint, including
// non-convex (L/T/U-shaped) ones -- the standard, robust construction real hip-roof generators
// (CityEngine, Blender's building-tool add-ons, QGIS hip-roof plugins) use, rather than the simpler
// "single ridge line" approximation FGrammarRoofFrameMath/GrammarRoof.cpp's HippedRoofMesh used
// before this class existed (still used for Gabled, which needs a straight ridge with vertical
// gable-end walls -- not what a plain straight skeleton produces).
//
// Every polygon edge is treated as a wavefront receding inward at unit speed, perpendicular to
// itself; each vertex moves at constant velocity along the bisector of its two adjacent (fixed-
// direction -- edges never rotate, only translate inward) edges' outward normals. Two kinds of
// events can occur before the whole polygon collapses to nothing:
//   - Edge event: a wavefront edge shrinks to zero length -- its two endpoints merge into one new
//     node, and the roof face for that edge's original polygon edge is finished (apex or ridge
//     point reached).
//   - Split event: a reflex (concave) vertex's swept path collides with a DIFFERENT, non-adjacent
//     wavefront edge before either of that edge's own endpoints reach it -- this is exactly what
//     happens at an L-shaped footprint's inner corner. The colliding edge is cut in two at the
//     collision point, each half continuing as its own independent wavefront edge, and the original
//     polygon's interior splits into two disjoint sub-regions that keep shrinking independently.
// Events are found by scanning all currently-active wavefront edges/vertices each step (this
// implementation deliberately favors a simple, easy-to-verify O(active-events^2) rescan per event
// over a priority-queue-with-invalidation approach -- building footprints have at most a few dozen
// vertices, so the extra constant factor is irrelevant, and getting split-event bookkeeping right is
// hard enough without also debugging a lazy-invalidation queue).
//
// This is pure 2D geometry with no notion of "roof" (eave height, ridge height, pitch, material) --
// FNode::Distance is purely "how far this point has receded from the original polygon boundary,
// measured as inward offset distance" (NOT a height); GrammarRoof.cpp's HippedRoofMesh converts
// Distance to Z (applying eave height and a pitch) and turns FFace into actual mesh triangles via
// FGrammarPolygonTriangulator.
class BUILDINGGRAMMARCORE_API FGrammarRoofSkeleton
{
public:
	struct FNode
	{
		FVector2D Position = FVector2D::ZeroVector;
		// Perpendicular distance this point has receded from the original polygon boundary --
		// proportional to roof height at unit pitch (see this class's own header comment). Zero for
		// the original footprint's own vertices.
		double Distance = 0.0;
	};

	// One roof face -- a simple (non-self-intersecting), CCW-wound polygon (consistent with the rest
	// of this module's footprint convention) whose lower edge is (a portion of) one original
	// footprint edge and whose remaining edges are skeleton edges, suitable for direct ear-clip
	// triangulation (FGrammarPolygonTriangulator) once the caller has converted each referenced
	// node's Distance into a Z height.
	struct FFace
	{
		TArray<int32> NodeIndices;
		// Which original Footprint edge (Footprint[i] -> Footprint[i+1]) this face's lower edge
		// derives from -- a source edge that was later cut by a split event produces more than one
		// FFace sharing the same SourceEdgeIndex.
		int32 SourceEdgeIndex = INDEX_NONE;
	};

	struct FResult
	{
		TArray<FNode> Nodes;
		TArray<FFace> Faces;
		// One entry per surviving flat-top loop, each a closed CCW ring of node indices, present
		// only for a loop whose sweep was still active when MaxDistance capped it (see Build's
		// MaxDistance parameter) -- i.e. the flat plateau top(s) of a capped hip roof. Empty if every
		// loop fully converged to a point/ridge before MaxDistance, or if MaxDistance was never
		// reached at all.
		TArray<TArray<int32>> TopRings;
	};

	// Computes the (possibly capped) straight skeleton of Footprint, a simple CCW polygon with no
	// repeated/collinear-degenerate points (see FGrammarGeometry2D::OrientFootprintCCW/
	// CleanFootprint -- callers are expected to have already run these). MaxDistance caps the sweep:
	// once every remaining loop's next event would occur beyond MaxDistance, the simulation stops
	// and each still-active loop is frozen at MaxDistance as a TopRings entry instead of continuing
	// to a peak/ridge. Pass TNumericLimits<double>::Max() for an uncapped skeleton that always fully
	// converges (no TopRings).
	//
	// Returns false (Nodes/Faces/TopRings all left empty) only for degenerate input (fewer than 3
	// points) -- callers should have a fallback for this, matching how FGrammarPolygonTriangulator
	// degrades gracefully rather than asserting.
	static bool Build(const TArray<FVector2D>& Footprint, double MaxDistance, FResult& OutResult);
};
