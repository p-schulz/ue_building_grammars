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
	Result.Reserve(Footprint.Num());

	if (Overhang <= 0.0)
	{
		for (const FVector2D& Point : Footprint)
		{
			Result.Add(FVector(Point.X, Point.Y, Height));
		}
		return Result;
	}

	const FVector2D Center = FGrammarGeometry2D::Centroid2D(Footprint);
	for (const FVector2D& Point : Footprint)
	{
		const FVector2D Delta = Point - Center;
		const double Length = Delta.Size();
		if (Length <= 0.0)
		{
			Result.Add(FVector(Point.X, Point.Y, Height));
		}
		else
		{
			const FVector2D Pushed = Point + Delta / Length * Overhang;
			Result.Add(FVector(Pushed.X, Pushed.Y, Height));
		}
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
