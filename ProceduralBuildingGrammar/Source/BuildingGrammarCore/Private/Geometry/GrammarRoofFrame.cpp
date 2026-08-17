#include "Geometry/GrammarRoofFrame.h"
#include "Geometry/GrammarGeometry2D.h"

namespace
{
	TArray<FVector2D> ToXY(const TArray<FVector>& Points)
	{
		TArray<FVector2D> Result;
		Result.Reserve(Points.Num());
		for (const FVector& Point : Points)
		{
			Result.Add(FVector2D(Point.X, Point.Y));
		}
		return Result;
	}
}

TArray<FVector> FGrammarRoofFrameMath::RoofBaseVertices(const TArray<FVector2D>& Footprint, double Height, double Overhang)
{
	TArray<FVector> Result;
	const int32 Count = Footprint.Num();
	Result.Reserve(Count);

	if (Overhang <= 0.0 || Count < 3)
	{
		for (const FVector2D& Point : Footprint)
		{
			Result.Add(FVector(Point.X, Point.Y, Height));
		}
		return Result;
	}

	// Per-vertex miter-join offset: each vertex is pushed along the bisector of its two adjacent
	// edges' own outward normals (FGrammarGeometry2D::OutwardNormal, reused rather than
	// re-derived), scaled so both adjacent edges end up exactly Overhang away from their original
	// line -- the standard polygon-offset construction, and correct for non-convex (L-shaped)
	// footprints. Previously this pushed each vertex radially away from the polygon's centroid
	// instead, which is only a good approximation for roughly-convex/rectangular footprints: at a
	// concave (inward) corner of an L-shaped building the centroid direction isn't aligned with
	// either adjacent edge's normal at all, so that eave vertex ended up shifted sideways off its
	// wall edge instead of pushed straight out from it. Assumes Footprint is CCW (every caller
	// already guarantees this -- see FGrammarGeometry2D::OrientFootprintCCW's own callers).
	//
	// Not a full robust polygon-offset (no self-intersection clipping for extreme reflex angles or
	// very large Overhang relative to edge length, unlike e.g. Clipper's polygon offsetting) --
	// building footprints have small Overhang relative to wall lengths in practice, so a plain
	// miter join (with its length clamped to at most 4x Overhang, avoiding an unbounded spike at a
	// near-180-degree corner) is sufficient without pulling in a general polygon-clipping library.
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector2D& Prev = Footprint[(Index - 1 + Count) % Count];
		const FVector2D& Current = Footprint[Index];
		const FVector2D& Next = Footprint[(Index + 1) % Count];

		const FVector2D NormalIn = FGrammarGeometry2D::OutwardNormal(Prev, Current, /*bCCW=*/true);
		const FVector2D NormalOut = FGrammarGeometry2D::OutwardNormal(Current, Next, /*bCCW=*/true);

		FVector2D Bisector = FGrammarGeometry2D::Normalize2D(NormalIn + NormalOut);
		double MiterScale = 1.0;
		if (Bisector.IsNearlyZero())
		{
			// Adjacent edges fold back on themselves (near-180-degree turn, or one edge is
			// degenerate/zero-length) -- fall back to whichever normal is valid so the offset
			// direction stays sane instead of collapsing to zero.
			Bisector = NormalIn.IsNearlyZero() ? NormalOut : NormalIn;
		}
		else
		{
			const double CosHalfAngle = FVector2D::DotProduct(Bisector, NormalIn);
			MiterScale = FMath::Clamp(1.0 / FMath::Max(CosHalfAngle, 0.25), 1.0, 4.0);
		}

		const FVector2D Pushed = Current + Bisector * (Overhang * MiterScale);
		Result.Add(FVector(Pushed.X, Pushed.Y, Height));
	}
	return Result;
}

FGrammarRoofFrame FGrammarRoofFrameMath::BuildFrame(const TArray<FVector>& Base, FVector2D Direction, double EaveZ, double RidgeZ)
{
	const TArray<FVector2D> BaseXY = ToXY(Base);

	FVector2D NormalizedDirection = FGrammarGeometry2D::Normalize2D(Direction);
	if (NormalizedDirection.IsNearlyZero())
	{
		NormalizedDirection = FGrammarGeometry2D::LongestAxisDirection(BaseXY);
	}
	const FVector2D Normal(-NormalizedDirection.Y, NormalizedDirection.X);
	const FVector2D Center = FGrammarGeometry2D::Centroid2D(BaseXY);

	FGrammarRoofFrame Frame;
	Frame.Center = Center;
	Frame.Direction = NormalizedDirection;
	Frame.Normal = Normal;
	Frame.EaveZ = EaveZ;
	Frame.RidgeZ = RidgeZ;

	Frame.MinLong = TNumericLimits<double>::Max();
	Frame.MaxLong = TNumericLimits<double>::Lowest();
	Frame.MinSide = TNumericLimits<double>::Max();
	Frame.MaxSide = TNumericLimits<double>::Lowest();
	for (const FVector2D& Point : BaseXY)
	{
		const double LongValue = AxisValue(Point, Center, NormalizedDirection);
		const double SideValue = AxisValue(Point, Center, Normal);
		Frame.MinLong = FMath::Min(Frame.MinLong, LongValue);
		Frame.MaxLong = FMath::Max(Frame.MaxLong, LongValue);
		Frame.MinSide = FMath::Min(Frame.MinSide, SideValue);
		Frame.MaxSide = FMath::Max(Frame.MaxSide, SideValue);
	}
	return Frame;
}

double FGrammarRoofFrameMath::AxisValue(const FVector2D& Point, const FVector2D& Center, const FVector2D& Axis)
{
	return (Point.X - Center.X) * Axis.X + (Point.Y - Center.Y) * Axis.Y;
}

FVector FGrammarRoofFrameMath::PointFromRoofAxes(const FGrammarRoofFrame& Frame, double LongValue, double SideValue, double Z)
{
	return FVector(
		Frame.Center.X + Frame.Direction.X * LongValue + Frame.Normal.X * SideValue,
		Frame.Center.Y + Frame.Direction.Y * LongValue + Frame.Normal.Y * SideValue,
		Z);
}

double FGrammarRoofFrameMath::RoofSurfaceZ(double LongValue, double SideValue, const FGrammarRoofFrame& Frame)
{
	(void)LongValue; // Deliberately unused -- see the gable-cross-section note on FGrammarRoofFrame.
	const double HalfSpan = FMath::Max3(FMath::Abs(Frame.MinSide), FMath::Abs(Frame.MaxSide), 1e-6);
	const double Slope = 1.0 - FMath::Min(FMath::Abs(SideValue) / HalfSpan, 1.0);
	return Frame.EaveZ + (Frame.RidgeZ - Frame.EaveZ) * Slope;
}

FVector FGrammarRoofFrameMath::RidgeProjection(const FVector2D& Point, const FGrammarRoofFrame& Frame)
{
	double Station = AxisValue(Point, Frame.Center, Frame.Direction);
	Station = FMath::Clamp(Station, Frame.MinLong, Frame.MaxLong);
	return PointFromRoofAxes(Frame, Station, 0.0, Frame.RidgeZ);
}

TArray<double> FGrammarRoofFrameMath::DetailPositions(int32 Count, double Minimum, double Maximum, double Inset)
{
	TArray<double> Positions;
	if (Count <= 0)
	{
		return Positions;
	}
	const double Span = Maximum - Minimum;
	if (Span <= 0.0)
	{
		Positions.Add((Minimum + Maximum) / 2.0);
		return Positions;
	}
	const double Start = Minimum + Span * Inset;
	const double End = Maximum - Span * Inset;
	if (Count == 1)
	{
		Positions.Add((Start + End) / 2.0);
		return Positions;
	}
	Positions.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Positions.Add(Start + (End - Start) * Index / static_cast<double>(Count - 1));
	}
	return Positions;
}
