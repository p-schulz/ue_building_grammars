#include "Geometry/GrammarWallRecess.h"
#include "Geometry/GrammarGeometry2D.h"

namespace
{
	// Real margin (meters) required on every side of an opening before it's cut -- an opening flush
	// against the wall's own boundary (or wider/taller than the wall) can't produce a real jamb/sill/
	// head, so it's left uncut rather than emitting degenerate near-zero-area geometry.
	constexpr double MinMargin = 0.02;

	// Two openings within this S tolerance (meters) on BOTH SLeft and SRight are treated as the exact
	// same column (e.g. the same window position repeated on several floors) rather than a partial
	// overlap -- see BuildSegments' dedup pass.
	constexpr double RangeEpsilon = 1e-4;

	FVector XYZAt(const FVector2D& Start, const FVector2D& Tangent, const FVector2D& Normal, double S, double D, double Z)
	{
		const FVector2D XY = FGrammarGeometry2D::PointOnSegment(Start, Tangent, Normal, S, D);
		return FVector(XY.X, XY.Y, Z);
	}

	// Every quad below is built from one of two verified identities (Tangent=U, OutwardNormal=N,
	// Up=Z): U x Z = N (established by the existing flush wall quad, which must face +N to be visible
	// from outside) and, derived from PCGFacadeWindowDoorLayout.cpp's already-verified "Tangent x
	// OutwardNormal = -Up" (i.e. U x N = -Z): N x Z = -U, N x U = Z. Each Make*Quad below picks the
	// corner order (lo1,lo2)-(hi1,lo2)-(hi1,hi2)-(lo1,hi2) for whichever pair of axes' cross product
	// equals the direction that quad needs to face.

	// Flush wall plane / recessed back plane (same shape, different D): faces +N. Template(S,Z) -> U x Z = N.
	FGrammarWallQuad MakeFlushQuad(const FVector2D& Start, const FVector2D& Tangent, const FVector2D& Normal, double S0, double S1, double Z0, double Z1, double D)
	{
		FGrammarWallQuad Quad;
		Quad.Corners[0] = XYZAt(Start, Tangent, Normal, S0, D, Z0);
		Quad.Corners[1] = XYZAt(Start, Tangent, Normal, S1, D, Z0);
		Quad.Corners[2] = XYZAt(Start, Tangent, Normal, S1, D, Z1);
		Quad.Corners[3] = XYZAt(Start, Tangent, Normal, S0, D, Z1);
		return Quad;
	}

	// Left jamb (at S=SLeft, bridging flush D=0 to recessed D=-RecessDepth): faces +U (into the
	// pocket, toward higher S). Template(Z,D) -> Z x N = U.
	FGrammarWallQuad MakeLeftJambQuad(const FVector2D& Start, const FVector2D& Tangent, const FVector2D& Normal, double S, double Z0, double Z1, double RecessDepth)
	{
		FGrammarWallQuad Quad;
		Quad.Corners[0] = XYZAt(Start, Tangent, Normal, S, -RecessDepth, Z0);
		Quad.Corners[1] = XYZAt(Start, Tangent, Normal, S, -RecessDepth, Z1);
		Quad.Corners[2] = XYZAt(Start, Tangent, Normal, S, 0.0, Z1);
		Quad.Corners[3] = XYZAt(Start, Tangent, Normal, S, 0.0, Z0);
		return Quad;
	}

	// Right jamb (at S=SRight): faces -U (into the pocket, toward lower S). Template(D,Z) -> N x Z = -U.
	FGrammarWallQuad MakeRightJambQuad(const FVector2D& Start, const FVector2D& Tangent, const FVector2D& Normal, double S, double Z0, double Z1, double RecessDepth)
	{
		FGrammarWallQuad Quad;
		Quad.Corners[0] = XYZAt(Start, Tangent, Normal, S, -RecessDepth, Z0);
		Quad.Corners[1] = XYZAt(Start, Tangent, Normal, S, 0.0, Z0);
		Quad.Corners[2] = XYZAt(Start, Tangent, Normal, S, 0.0, Z1);
		Quad.Corners[3] = XYZAt(Start, Tangent, Normal, S, -RecessDepth, Z1);
		return Quad;
	}

	// Sill (floor of the pocket, at Z=ZBottom): faces +Z (up). Template(D,S) -> N x U = Z.
	FGrammarWallQuad MakeSillQuad(const FVector2D& Start, const FVector2D& Tangent, const FVector2D& Normal, double S0, double S1, double Z, double RecessDepth)
	{
		FGrammarWallQuad Quad;
		Quad.Corners[0] = XYZAt(Start, Tangent, Normal, S0, -RecessDepth, Z);
		Quad.Corners[1] = XYZAt(Start, Tangent, Normal, S0, 0.0, Z);
		Quad.Corners[2] = XYZAt(Start, Tangent, Normal, S1, 0.0, Z);
		Quad.Corners[3] = XYZAt(Start, Tangent, Normal, S1, -RecessDepth, Z);
		return Quad;
	}

	// Head (ceiling of the pocket, at Z=ZTop): faces -Z (down). Template(S,D) -> U x N = -Z.
	FGrammarWallQuad MakeHeadQuad(const FVector2D& Start, const FVector2D& Tangent, const FVector2D& Normal, double S0, double S1, double Z, double RecessDepth)
	{
		FGrammarWallQuad Quad;
		Quad.Corners[0] = XYZAt(Start, Tangent, Normal, S0, -RecessDepth, Z);
		Quad.Corners[1] = XYZAt(Start, Tangent, Normal, S1, -RecessDepth, Z);
		Quad.Corners[2] = XYZAt(Start, Tangent, Normal, S1, 0.0, Z);
		Quad.Corners[3] = XYZAt(Start, Tangent, Normal, S0, 0.0, Z);
		return Quad;
	}
}

TArray<FGrammarWallQuad> FGrammarWallRecess::BuildSegments(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double WallBottom, double WallTop, TArray<FGrammarWallOpening> Openings)
{
	TArray<FGrammarWallQuad> Result;
	const double Length = FGrammarGeometry2D::Distance2D(Start, End);
	if (Length <= 0.0 || WallTop <= WallBottom)
	{
		return Result;
	}
	const FVector2D Tangent = FGrammarGeometry2D::Tangent(Start, End);

	TArray<FGrammarWallOpening> Valid;
	for (const FGrammarWallOpening& Opening : Openings)
	{
		if (Opening.RecessDepth <= 0.0) continue;
		if (Opening.SLeft <= MinMargin || Opening.SRight >= Length - MinMargin) continue;
		if (Opening.ZBottom <= WallBottom + MinMargin || Opening.ZTop >= WallTop - MinMargin) continue;
		if (Opening.SRight - Opening.SLeft < MinMargin || Opening.ZTop - Opening.ZBottom < MinMargin) continue;
		Valid.Add(Opening);
	}

	if (Valid.Num() == 0)
	{
		Result.Add(MakeFlushQuad(Start, Tangent, Normal, 0.0, Length, WallBottom, WallTop, 0.0));
		return Result;
	}

	Valid.Sort([](const FGrammarWallOpening& A, const FGrammarWallOpening& B)
	{
		return A.SLeft != B.SLeft ? A.SLeft < B.SLeft : A.SRight < B.SRight;
	});

	// Drop any opening whose S-range partially overlaps an already-accepted DISTINCT range -- guards
	// against self-intersecting geometry rather than trusting callers' spacing blindly. An opening
	// sharing the exact same [SLeft,SRight] as the last accepted one is intentionally kept (that's the
	// "same window column repeated on several floors" stacking case BuildSegments is designed for, not
	// a conflict).
	TArray<FGrammarWallOpening> NonOverlapping;
	double AcceptedSLeft = 0.0;
	double AcceptedSRight = TNumericLimits<double>::Lowest();
	for (const FGrammarWallOpening& Opening : Valid)
	{
		const bool bMatchesAcceptedRange = NonOverlapping.Num() > 0
			&& FMath::Abs(Opening.SLeft - AcceptedSLeft) < RangeEpsilon
			&& FMath::Abs(Opening.SRight - AcceptedSRight) < RangeEpsilon;
		const bool bNoOverlapWithAccepted = NonOverlapping.Num() == 0 || Opening.SLeft >= AcceptedSRight - RangeEpsilon;
		if (bMatchesAcceptedRange || bNoOverlapWithAccepted)
		{
			NonOverlapping.Add(Opening);
			AcceptedSLeft = Opening.SLeft;
			AcceptedSRight = Opening.SRight;
		}
	}

	// Vertical-strip sweep: every distinct S boundary contributed by an opening's own SLeft/SRight
	// (plus the wall's own ends) splits the wall into strips that either lie fully outside every
	// opening (a full-height flush quad) or exactly span one opening column's S-range -- which may
	// still need Z-banding for multiple openings stacked at that column (several floors' worth).
	TArray<double> Boundaries;
	Boundaries.Add(0.0);
	Boundaries.Add(Length);
	for (const FGrammarWallOpening& Opening : NonOverlapping)
	{
		Boundaries.Add(Opening.SLeft);
		Boundaries.Add(Opening.SRight);
	}
	Boundaries.Sort();

	for (int32 Index = 0; Index + 1 < Boundaries.Num(); ++Index)
	{
		const double S0 = Boundaries[Index];
		const double S1 = Boundaries[Index + 1];
		if (S1 - S0 < KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const double Mid = (S0 + S1) * 0.5;

		TArray<FGrammarWallOpening> Stack;
		for (const FGrammarWallOpening& Opening : NonOverlapping)
		{
			if (Opening.SLeft <= Mid && Mid <= Opening.SRight)
			{
				Stack.Add(Opening);
			}
		}

		if (Stack.Num() == 0)
		{
			Result.Add(MakeFlushQuad(Start, Tangent, Normal, S0, S1, WallBottom, WallTop, 0.0));
			continue;
		}

		Stack.Sort([](const FGrammarWallOpening& A, const FGrammarWallOpening& B) { return A.ZBottom < B.ZBottom; });

		double CurrentZ = WallBottom;
		for (const FGrammarWallOpening& Opening : Stack)
		{
			if (Opening.ZBottom > CurrentZ)
			{
				Result.Add(MakeFlushQuad(Start, Tangent, Normal, S0, S1, CurrentZ, Opening.ZBottom, 0.0));
			}
			Result.Add(MakeFlushQuad(Start, Tangent, Normal, S0, S1, Opening.ZBottom, Opening.ZTop, -Opening.RecessDepth));
			Result.Add(MakeLeftJambQuad(Start, Tangent, Normal, S0, Opening.ZBottom, Opening.ZTop, Opening.RecessDepth));
			Result.Add(MakeRightJambQuad(Start, Tangent, Normal, S1, Opening.ZBottom, Opening.ZTop, Opening.RecessDepth));
			Result.Add(MakeSillQuad(Start, Tangent, Normal, S0, S1, Opening.ZBottom, Opening.RecessDepth));
			Result.Add(MakeHeadQuad(Start, Tangent, Normal, S0, S1, Opening.ZTop, Opening.RecessDepth));
			CurrentZ = Opening.ZTop;
		}
		if (CurrentZ < WallTop)
		{
			Result.Add(MakeFlushQuad(Start, Tangent, Normal, S0, S1, CurrentZ, WallTop, 0.0));
		}
	}

	return Result;
}
