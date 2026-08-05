#include "Geometry/GrammarPolygonTriangulator.h"

namespace
{
	double Cross2D(const FVector2D& O, const FVector2D& A, const FVector2D& B)
	{
		return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X);
	}

	// True if P is inside-or-on triangle ABC (2D). Used to reject ear candidates that would trap
	// another remaining vertex -- boundary counts as "inside" (blocks the ear) to stay conservative.
	bool PointInOrOnTriangle(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		const double D1 = Cross2D(A, B, P);
		const double D2 = Cross2D(B, C, P);
		const double D3 = Cross2D(C, A, P);
		const bool bHasNeg = (D1 < 0.0) || (D2 < 0.0) || (D3 < 0.0);
		const bool bHasPos = (D1 > 0.0) || (D2 > 0.0) || (D3 > 0.0);
		return !(bHasNeg && bHasPos);
	}

	double SignedArea2D(const TArray<FVector2D>& Points)
	{
		double Area = 0.0;
		const int32 Count = Points.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D& Current = Points[Index];
			const FVector2D& Next = Points[(Index + 1) % Count];
			Area += Current.X * Next.Y - Next.X * Current.Y;
		}
		return Area / 2.0;
	}

	// Newell's method: robust against collinear/near-degenerate leading vertices, unlike a plain
	// 3-point cross product.
	FVector FacePlaneNormal(const TArray<FVector>& Positions)
	{
		FVector Normal = FVector::ZeroVector;
		const int32 Count = Positions.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector& Current = Positions[Index];
			const FVector& Next = Positions[(Index + 1) % Count];
			Normal.X += (Current.Y - Next.Y) * (Current.Z + Next.Z);
			Normal.Y += (Current.Z - Next.Z) * (Current.X + Next.X);
			Normal.Z += (Current.X - Next.X) * (Current.Y + Next.Y);
		}
		return Normal.GetSafeNormal();
	}

	TArray<int32> FanTriangulate(const FGrammarFace& Face)
	{
		TArray<int32> Triangles;
		const int32 Count = Face.Indices.Num();
		for (int32 Index = 1; Index + 1 < Count; ++Index)
		{
			Triangles.Add(Face.Indices[0]);
			Triangles.Add(Face.Indices[Index]);
			Triangles.Add(Face.Indices[Index + 1]);
		}
		return Triangles;
	}
}

TArray<int32> FGrammarPolygonTriangulator::Triangulate(const TArray<FVector>& Vertices, const FGrammarFace& Face)
{
	const int32 Count = Face.Indices.Num();
	if (Count < 3)
	{
		return {};
	}
	if (Count == 3)
	{
		return Face.Indices;
	}

	TArray<FVector> Positions;
	Positions.Reserve(Count);
	for (const int32 Index : Face.Indices)
	{
		Positions.Add(Vertices[Index]);
	}

	const FVector Normal = FacePlaneNormal(Positions);
	if (Normal.IsNearlyZero())
	{
		// Degenerate (collinear or zero-area) face -- fall back to a fan rather than producing no
		// geometry at all.
		return FanTriangulate(Face);
	}

	FVector AxisU, AxisV;
	Normal.FindBestAxisVectors(AxisU, AxisV);

	TArray<FVector2D> Local2D;
	Local2D.Reserve(Count);
	for (const FVector& Position : Positions)
	{
		const FVector Relative = Position - Positions[0];
		Local2D.Add(FVector2D(FVector::DotProduct(Relative, AxisU), FVector::DotProduct(Relative, AxisV)));
	}

	// Ear-clipping needs a consistent (CCW) winding to classify convex vs. reflex vertices.
	TArray<int32> Order;
	Order.Reserve(Count);
	if (SignedArea2D(Local2D) >= 0.0)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Order.Add(Index);
		}
	}
	else
	{
		for (int32 Index = Count - 1; Index >= 0; --Index)
		{
			Order.Add(Index);
		}
	}

	TArray<int32> Triangles;
	Triangles.Reserve((Count - 2) * 3);

	while (Order.Num() > 3)
	{
		const int32 RemainingCount = Order.Num();
		bool bFoundEar = false;

		for (int32 EarIndex = 0; EarIndex < RemainingCount; ++EarIndex)
		{
			const int32 PrevIndex = Order[(EarIndex - 1 + RemainingCount) % RemainingCount];
			const int32 CurrIndex = Order[EarIndex];
			const int32 NextIndex = Order[(EarIndex + 1) % RemainingCount];

			const FVector2D& A = Local2D[PrevIndex];
			const FVector2D& B = Local2D[CurrIndex];
			const FVector2D& C = Local2D[NextIndex];

			// Convex (left turn) test for a CCW polygon.
			if (Cross2D(A, B, C) <= 0.0)
			{
				continue;
			}

			bool bContainsOtherVertex = false;
			for (int32 OtherIndex = 0; OtherIndex < RemainingCount; ++OtherIndex)
			{
				const int32 Candidate = Order[OtherIndex];
				if (Candidate == PrevIndex || Candidate == CurrIndex || Candidate == NextIndex)
				{
					continue;
				}
				if (PointInOrOnTriangle(Local2D[Candidate], A, B, C))
				{
					bContainsOtherVertex = true;
					break;
				}
			}
			if (bContainsOtherVertex)
			{
				continue;
			}

			Triangles.Add(Face.Indices[PrevIndex]);
			Triangles.Add(Face.Indices[CurrIndex]);
			Triangles.Add(Face.Indices[NextIndex]);
			Order.RemoveAt(EarIndex);
			bFoundEar = true;
			break;
		}

		if (!bFoundEar)
		{
			// Self-intersecting or otherwise pathological input -- fan the remainder so we still
			// produce *some* geometry instead of looping forever.
			for (int32 Index = 1; Index + 1 < Order.Num(); ++Index)
			{
				Triangles.Add(Face.Indices[Order[0]]);
				Triangles.Add(Face.Indices[Order[Index]]);
				Triangles.Add(Face.Indices[Order[Index + 1]]);
			}
			Order.Reset();
			break;
		}
	}

	if (Order.Num() == 3)
	{
		Triangles.Add(Face.Indices[Order[0]]);
		Triangles.Add(Face.Indices[Order[1]]);
		Triangles.Add(Face.Indices[Order[2]]);
	}

	return Triangles;
}
