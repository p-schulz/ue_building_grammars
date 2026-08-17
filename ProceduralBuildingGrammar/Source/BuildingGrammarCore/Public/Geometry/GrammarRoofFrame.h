#pragma once

#include "CoreMinimal.h"

// Port of grammar.py's RoofFrame + associated free functions. A RoofFrame is a 2D "roof-local"
// coordinate frame -- Direction is the ridge/long axis, Normal is perpendicular to it -- that every
// roof-detail placement function (tiles, dormers, roof windows, chimneys) works in.
//
// Known behavior carried over deliberately (see the plugin's plan doc, "Known Behavior
// Decisions"): RoofSurfaceZ is a pure gable cross-section (a function of SideValue only, ignoring
// LongValue entirely) used for detail *placement* regardless of roof type -- so hipped/pyramid
// roofs place tiles/dormers/chimneys as if gabled even though their roof *plane* mesh (built
// separately, not by this struct) is correctly hipped/pyramidal. Replicated here for parity with
// the Blender add-on; revisit once visual impact is confirmed.
struct BUILDINGGRAMMARCORE_API FGrammarRoofFrame
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D Direction = FVector2D(1.0, 0.0);
	FVector2D Normal = FVector2D(0.0, 1.0);
	double MinLong = 0.0;
	double MaxLong = 0.0;
	double MinSide = 0.0;
	double MaxSide = 0.0;
	double EaveZ = 0.0;
	double RidgeZ = 0.0;
};

class BUILDINGGRAMMARCORE_API FGrammarRoofFrameMath
{
public:
	// Offsets the footprint outward by Overhang via a per-vertex miter join (each vertex pushed
	// along the bisector of its two adjacent edges' outward normals) -- correct for non-convex
	// (L-shaped) footprints, unlike a simpler radial-from-centroid push. Height becomes every
	// output vertex's Z. See the .cpp for the full construction and its known limits (no
	// self-intersection clipping for extreme reflex corners).
	static TArray<FVector> RoofBaseVertices(const TArray<FVector2D>& Footprint, double Height, double Overhang);

	// Normalizes Direction (falling back to the base's longest AABB axis if zero-length), derives
	// Normal, and projects every base vertex onto Direction/Normal relative to the centroid to get
	// the frame's long/side extents.
	static FGrammarRoofFrame BuildFrame(const TArray<FVector>& Base, FVector2D Direction, double EaveZ, double RidgeZ);

	static double AxisValue(const FVector2D& Point, const FVector2D& Center, const FVector2D& Axis);

	static FVector PointFromRoofAxes(const FGrammarRoofFrame& Frame, double LongValue, double SideValue, double Z);

	// Gable cross-section height at a given lateral offset -- see the class-level comment above.
	static double RoofSurfaceZ(double LongValue, double SideValue, const FGrammarRoofFrame& Frame);

	// Clamps Point's long-axis station into [MinLong, MaxLong] and projects it onto the ridge line
	// at RidgeZ (used by the gabled roof-plane mesh, not the detail-placement path above). Equivalent
	// to ProjectAtSideAndHeight(Point, Frame, 0.0, Frame.RidgeZ) below.
	static FVector RidgeProjection(const FVector2D& Point, const FGrammarRoofFrame& Frame);

	// Generalization of RidgeProjection: clamps Point's long-axis station into [MinLong, MaxLong] the
	// same way, but projects it onto an arbitrary (SideValue, Z) line instead of the fixed ridge
	// (Side=0, RidgeZ) -- used by the gambrel roof-plane mesh's intermediate break line, which sits
	// off-center (partway between ridge and eave) at a height between eave and ridge.
	static FVector ProjectAtSideAndHeight(const FVector2D& Point, const FGrammarRoofFrame& Frame, double SideValue, double Z);

	// Evenly-spaced-with-inset station list along the long axis, reused by dormer/roof-window/
	// chimney placement. Count<=0 -> empty; degenerate span -> single midpoint.
	static TArray<double> DetailPositions(int32 Count, double Minimum, double Maximum, double Inset);
};
