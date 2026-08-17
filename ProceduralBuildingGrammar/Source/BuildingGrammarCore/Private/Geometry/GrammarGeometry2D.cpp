#include "Geometry/GrammarGeometry2D.h"
#include "Algo/Reverse.h"

namespace
{
	constexpr double GeometryEpsilon = 1e-9;
}

TArray<FVector2D> FGrammarGeometry2D::CleanFootprint(const TArray<FVector2D>& Footprint)
{
	TArray<FVector2D> Clean;
	Clean.Reserve(Footprint.Num());
	for (const FVector2D& Point : Footprint)
	{
		if (Clean.Num() == 0 || !Clean.Last().Equals(Point, KINDA_SMALL_NUMBER))
		{
			Clean.Add(Point);
		}
	}
	if (Clean.Num() > 1 && Clean[0].Equals(Clean.Last(), KINDA_SMALL_NUMBER))
	{
		Clean.Pop(EAllowShrinking::No);
	}
	return Clean;
}

double FGrammarGeometry2D::SignedPolygonArea(const TArray<FVector2D>& Footprint)
{
	const TArray<FVector2D> Clean = CleanFootprint(Footprint);
	if (Clean.Num() < 3)
	{
		return 0.0;
	}
	double Area = 0.0;
	for (const FEdge& Edge : GetSegments(Clean))
	{
		Area += Edge.Start.X * Edge.End.Y - Edge.End.X * Edge.Start.Y;
	}
	return Area / 2.0;
}

bool FGrammarGeometry2D::PolygonIsCCW(const TArray<FVector2D>& Footprint)
{
	return SignedPolygonArea(Footprint) > 0.0;
}

TArray<FVector2D> FGrammarGeometry2D::OrientFootprintCCW(const TArray<FVector2D>& Footprint)
{
	TArray<FVector2D> Clean = CleanFootprint(Footprint);
	if (Clean.Num() < 3 || PolygonIsCCW(Clean))
	{
		return Clean;
	}
	Algo::Reverse(Clean);
	return Clean;
}

TArray<FGrammarGeometry2D::FEdge> FGrammarGeometry2D::GetSegments(const TArray<FVector2D>& Ring)
{
	TArray<FEdge> Segments;
	const int32 Count = Ring.Num();
	Segments.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FEdge Edge;
		Edge.Start = Ring[Index];
		Edge.End = Ring[(Index + 1) % Count];
		Segments.Add(Edge);
	}
	return Segments;
}

FVector2D FGrammarGeometry2D::OutwardNormal(const FVector2D& Start, const FVector2D& End, bool bCCW)
{
	const double DX = End.X - Start.X;
	const double DY = End.Y - Start.Y;
	const double Length = FMath::Sqrt(DX * DX + DY * DY);
	if (Length <= GeometryEpsilon)
	{
		return FVector2D::ZeroVector;
	}
	if (bCCW)
	{
		return FVector2D(DY / Length, -DX / Length);
	}
	return FVector2D(-DY / Length, DX / Length);
}

FVector2D FGrammarGeometry2D::Tangent(const FVector2D& Start, const FVector2D& End)
{
	return Normalize2D(End - Start);
}

FVector2D FGrammarGeometry2D::PointOnSegment(const FVector2D& Start, const FVector2D& Tangent, const FVector2D& Normal, double OffsetAlongEdge, double DepthOutward)
{
	return Start + Tangent * OffsetAlongEdge + Normal * DepthOutward;
}

FVector2D FGrammarGeometry2D::Move(const FVector2D& Point, const FVector2D& Direction, double Distance)
{
	return Point + Direction * Distance;
}

FVector2D FGrammarGeometry2D::OffsetPoint(const FVector2D& Point, const FVector2D& Normal, double Distance)
{
	return Point + Normal * Distance;
}

double FGrammarGeometry2D::Distance2D(const FVector2D& A, const FVector2D& B)
{
	return FVector2D::Distance(A, B);
}

FVector2D FGrammarGeometry2D::Centroid2D(const TArray<FVector2D>& Points)
{
	if (Points.Num() == 0)
	{
		return FVector2D::ZeroVector;
	}
	FVector2D Sum = FVector2D::ZeroVector;
	for (const FVector2D& Point : Points)
	{
		Sum += Point;
	}
	return Sum / static_cast<double>(Points.Num());
}

FVector2D FGrammarGeometry2D::Normalize2D(const FVector2D& V)
{
	const double Length = V.Size();
	if (Length <= GeometryEpsilon)
	{
		return FVector2D::ZeroVector;
	}
	return V / Length;
}

FBox2D FGrammarGeometry2D::Bounds(const TArray<FVector2D>& Points)
{
	FBox2D Result(ForceInit);
	for (const FVector2D& Point : Points)
	{
		Result += Point;
	}
	return Result;
}

FVector2D FGrammarGeometry2D::LongestAxisDirection(const TArray<FVector2D>& Points)
{
	// The direction of Points' own longest boundary edge (treated as a closed ring -- same convention
	// as GetSegments: (P0,P1), (P1,P2), ..., (Pn-1,P0)), NOT the wider axis of the world-axis-aligned
	// bounding box. An earlier version of this function compared Box.GetExtent().X/Y and snapped to
	// world (1,0)/(0,1) -- correct only for footprints already aligned to North/East, and silently
	// rotating every gabled/hipped roof's ridge (plus every roof-tile/dormer/chimney placement that
	// shares this function -- see GrammarRoofDetails.cpp) to world coordinates for any footprint
	// rotated relative to them, regardless of the building's own shape.
	if (Points.Num() < 2)
	{
		return FVector2D(1.0, 0.0);
	}

	double BestLengthSquared = -1.0;
	FVector2D BestEdge(1.0, 0.0);
	const int32 Count = Points.Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector2D Edge = Points[(Index + 1) % Count] - Points[Index];
		const double LengthSquared = Edge.SizeSquared();
		if (LengthSquared > BestLengthSquared)
		{
			BestLengthSquared = LengthSquared;
			BestEdge = Edge;
		}
	}

	const FVector2D Normalized = Normalize2D(BestEdge);
	return Normalized.IsNearlyZero() ? FVector2D(1.0, 0.0) : Normalized;
}

double FGrammarGeometry2D::PointToSegmentDistanceSquared(const FVector2D& P, const FVector2D& A, const FVector2D& B)
{
	const FVector2D AB = B - A;
	const double LengthSquared = AB.SizeSquared();
	if (LengthSquared <= GeometryEpsilon)
	{
		return FVector2D::DistSquared(P, A);
	}
	const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / LengthSquared, 0.0, 1.0);
	const FVector2D Closest = A + AB * T;
	return FVector2D::DistSquared(P, Closest);
}

double FGrammarGeometry2D::PointToPolylineDistance(const FVector2D& P, const TArray<FVector2D>& Polyline)
{
	double Best = TNumericLimits<double>::Max();
	for (int32 Index = 1; Index < Polyline.Num(); ++Index)
	{
		Best = FMath::Min(Best, FMath::Sqrt(PointToSegmentDistanceSquared(P, Polyline[Index - 1], Polyline[Index])));
	}
	return Best;
}

TArray<double> FGrammarGeometry2D::FloorBottoms(const TArray<double>& FloorHeights)
{
	TArray<double> Bottoms;
	Bottoms.Reserve(FloorHeights.Num());
	double Current = 0.0;
	for (const double Height : FloorHeights)
	{
		Bottoms.Add(Current);
		Current += Height;
	}
	return Bottoms;
}

TArray<FGrammarGeometry2D::FPolylineSample> FGrammarGeometry2D::PointsAlongPolyline(const TArray<FVector2D>& Polyline, double Spacing, double StartOffset)
{
	TArray<FPolylineSample> Samples;
	if (Polyline.Num() < 2 || Spacing <= 0.0)
	{
		return Samples;
	}

	double NextTargetLength = StartOffset;
	double AccumulatedLength = 0.0;
	for (int32 Index = 0; Index + 1 < Polyline.Num(); ++Index)
	{
		const FVector2D& SegmentStart = Polyline[Index];
		const FVector2D& SegmentEnd = Polyline[Index + 1];
		const double SegmentLength = Distance2D(SegmentStart, SegmentEnd);
		if (SegmentLength <= GeometryEpsilon)
		{
			continue;
		}

		const FVector2D SegmentTangent = Tangent(SegmentStart, SegmentEnd);
		while (NextTargetLength <= AccumulatedLength + SegmentLength)
		{
			const double DistanceIntoSegment = NextTargetLength - AccumulatedLength;
			FPolylineSample Sample;
			Sample.Position = SegmentStart + SegmentTangent * DistanceIntoSegment;
			Sample.Tangent = SegmentTangent;
			Samples.Add(Sample);
			NextTargetLength += Spacing;
		}
		AccumulatedLength += SegmentLength;
	}
	return Samples;
}

bool FGrammarGeometry2D::PointInRing(const FVector2D& P, const TArray<FVector2D>& Ring)
{
	bool bInside = false;
	const int32 Count = Ring.Num();
	for (int32 Index = 0, PrevIndex = Count - 1; Index < Count; PrevIndex = Index++)
	{
		const FVector2D& A = Ring[Index];
		const FVector2D& B = Ring[PrevIndex];
		if (PointToSegmentDistanceSquared(P, A, B) <= GeometryEpsilon)
		{
			return true;
		}
		const bool bCrosses = (A.Y > P.Y) != (B.Y > P.Y);
		if (bCrosses && P.X < (B.X - A.X) * (P.Y - A.Y) / ((B.Y - A.Y) + 1e-30) + A.X)
		{
			bInside = !bInside;
		}
	}
	return bInside;
}
