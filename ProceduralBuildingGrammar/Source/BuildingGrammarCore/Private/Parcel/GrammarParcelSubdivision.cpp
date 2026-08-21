#include "Parcel/GrammarParcelSubdivision.h"
#include "Geometry/GrammarGeometry2D.h"
#include "Math/RandomStream.h"

namespace
{
	// FVector2D has no built-in 2D cross/perp-dot product -- GrammarGeometry2D.cpp doesn't use one
	// either, so this stays local rather than guessing at an FVector2D API.
	double Cross2D(const FVector2D& A, const FVector2D& B)
	{
		return A.X * B.Y - A.Y * B.X;
	}

	double Clamp01(double Value)
	{
		return FMath::Clamp(Value, 0.0, 1.0);
	}

	FVector2D Lerp2D(const FVector2D& A, const FVector2D& B, double T)
	{
		return A + (B - A) * T;
	}

	// FRandomStream::GetFraction() returns float; every caller here works in double, so these wrap
	// it rather than mixing precisions ad hoc at each call site.
	double RandomUniform01(FRandomStream& Stream)
	{
		return static_cast<double>(Stream.GetFraction());
	}

	double RandomSigned(FRandomStream& Stream)
	{
		return RandomUniform01(Stream) * 2.0 - 1.0;
	}
}

bool FGrammarParcelSubdivision::SegmentsIntersect(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
{
	const auto Sign = [](double V) -> int32 { return (V > 1e-9) - (V < -1e-9); };
	const int32 D1 = Sign(Cross2D(B - A, C - A));
	const int32 D2 = Sign(Cross2D(B - A, D - A));
	const int32 D3 = Sign(Cross2D(D - C, A - C));
	const int32 D4 = Sign(Cross2D(D - C, B - C));
	return D1 != 0 && D2 != 0 && D3 != 0 && D4 != 0 && D1 != D2 && D3 != D4;
}

bool FGrammarParcelSubdivision::IsSimplePolygon(const TArray<FVector2D>& Polygon, double MinArea)
{
	if (Polygon.Num() < 3)
	{
		return false;
	}
	if (FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Polygon)) <= MinArea)
	{
		return false;
	}
	const int32 N = Polygon.Num();
	for (int32 i = 0; i < N; ++i)
	{
		const FVector2D& A0 = Polygon[i];
		const FVector2D& A1 = Polygon[(i + 1) % N];
		for (int32 j = i + 1; j < N; ++j)
		{
			const bool bAdjacent = (j == i + 1) || (i == 0 && j == N - 1);
			if (bAdjacent)
			{
				continue;
			}
			if (SegmentsIntersect(A0, A1, Polygon[j], Polygon[(j + 1) % N]))
			{
				return false;
			}
		}
	}
	return true;
}

FVector2D FGrammarParcelSubdivision::InteriorAnchor(const TArray<FVector2D>& Polygon)
{
	if (Polygon.Num() < 3)
	{
		return Polygon.IsEmpty() ? FVector2D::ZeroVector : Polygon[0];
	}

	const FVector2D Centroid = FGrammarGeometry2D::Centroid2D(Polygon);
	if (FGrammarGeometry2D::PointInRing(Centroid, Polygon))
	{
		return Centroid;
	}

	double MinY = Polygon[0].Y, MaxY = Polygon[0].Y;
	for (const FVector2D& V : Polygon)
	{
		MinY = FMath::Min(MinY, V.Y);
		MaxY = FMath::Max(MaxY, V.Y);
	}

	FVector2D Best = Centroid;
	double BestSpan = -1.0;
	constexpr int32 ScanLines = 9;
	for (int32 S = 1; S <= ScanLines; ++S)
	{
		const double Y = MinY + (MaxY - MinY) * (static_cast<double>(S) / (ScanLines + 1));
		TArray<double> Xs;
		const int32 N = Polygon.Num();
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = Polygon[i];
			const FVector2D& B = Polygon[(i + 1) % N];
			if ((A.Y <= Y && B.Y > Y) || (B.Y <= Y && A.Y > Y))
			{
				const double T = (Y - A.Y) / (B.Y - A.Y);
				Xs.Add(A.X + T * (B.X - A.X));
			}
		}
		if (Xs.Num() < 2)
		{
			continue;
		}
		Xs.Sort();
		for (int32 i = 0; i + 1 < Xs.Num(); i += 2)
		{
			const double Span = Xs[i + 1] - Xs[i];
			if (Span > BestSpan)
			{
				BestSpan = Span;
				Best = FVector2D((Xs[i] + Xs[i + 1]) * 0.5, Y);
			}
		}
	}
	return Best;
}

namespace
{
	TArray<FVector2D> ScaleTowardCentroid(const TArray<FVector2D>& Polygon, double Scale)
	{
		// Anchored on a validated interior point rather than the raw centroid -- the shoelace
		// centroid of a concave polygon (e.g. an L-shaped block) can land outside it.
		const FVector2D Anchor = FGrammarParcelSubdivision::InteriorAnchor(Polygon);
		Scale = FMath::Clamp(Scale, 0.02, 0.98);
		TArray<FVector2D> Out;
		Out.Reserve(Polygon.Num());
		for (const FVector2D& V : Polygon)
		{
			Out.Add(Lerp2D(Anchor, V, Scale));
		}
		return Out;
	}

	// Converts a target inward "depth" into a uniform scale factor relative to an interior anchor.
	double DepthToScale(const TArray<FVector2D>& Polygon, const FVector2D& Anchor, double Depth)
	{
		double AvgRadius = 0.0;
		for (const FVector2D& V : Polygon)
		{
			AvgRadius += FGrammarGeometry2D::Distance2D(V, Anchor);
		}
		AvgRadius /= FMath::Max(1, Polygon.Num());
		return FMath::Clamp(1.0 - Depth / FMath::Max(1.0, AvgRadius), 0.04, 0.94);
	}

	// Produces a scaled-toward-anchor inner contour and validates it, escalating the scale toward
	// the boundary (larger scale = closer to the original, always simple in the limit) until it
	// finds a valid result or gives up. Naive uniform scaling of a concave polygon can still
	// self-intersect at small scales (deep notches fold over first).
	TArray<FVector2D> SafeInnerContour(const TArray<FVector2D>& Block, double TargetScale, double MaxScale = 0.85, int32 Steps = 6)
	{
		double Scale = Clamp01(TargetScale);
		MaxScale = Clamp01(MaxScale);
		const double StepSize = FMath::Max(1e-3, (MaxScale - Scale) / FMath::Max(1, Steps));
		for (int32 i = 0; i <= Steps; ++i)
		{
			const TArray<FVector2D> Candidate = ScaleTowardCentroid(Block, Scale);
			if (FGrammarParcelSubdivision::IsSimplePolygon(Candidate))
			{
				return Candidate;
			}
			Scale = FMath::Min(MaxScale, Scale + StepSize);
		}
		return {};
	}

	struct FParcelProjection
	{
		double Min = 0.0;
		double Max = 0.0;
		double Size = 0.0;
	};

	FParcelProjection Project(const TArray<FVector2D>& Polygon, const FVector2D& Axis)
	{
		FParcelProjection Pr;
		Pr.Min = TNumericLimits<double>::Max();
		Pr.Max = -TNumericLimits<double>::Max();
		for (const FVector2D& V : Polygon)
		{
			const double D = FVector2D::DotProduct(V, Axis);
			Pr.Min = FMath::Min(Pr.Min, D);
			Pr.Max = FMath::Max(Pr.Max, D);
		}
		Pr.Size = Pr.Max - Pr.Min;
		return Pr;
	}

	struct FParcelObb
	{
		FVector2D U = FVector2D(1.0, 0.0);
		FVector2D V = FVector2D(0.0, 1.0);
		FParcelProjection Pu;
		FParcelProjection Pv;
		double Area = 0.0;
	};

	FParcelObb OrientedBoundingBox(const TArray<FVector2D>& Polygon)
	{
		FParcelObb Best;
		Best.Area = TNumericLimits<double>::Max();
		const int32 N = Polygon.Num();
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = Polygon[i];
			const FVector2D& B = Polygon[(i + 1) % N];
			if ((B - A).SizeSquared() <= 1e-12)
			{
				continue; // Degenerate (duplicate-point) edge -- skip rather than derive a degenerate axis from it.
			}
			const FVector2D U = FGrammarGeometry2D::Normalize2D(B - A);
			const FVector2D V(-U.Y, U.X);
			const FParcelProjection Pu = Project(Polygon, U);
			const FParcelProjection Pv = Project(Polygon, V);
			const double Area = Pu.Size * Pv.Size;
			if (Area < Best.Area)
			{
				Best = FParcelObb{U, V, Pu, Pv, Area};
			}
		}
		return Best;
	}

	TArray<FVector2D> ObbCorners(const FParcelObb& Box)
	{
		return {
			Box.U * Box.Pu.Min + Box.V * Box.Pv.Min,
			Box.U * Box.Pu.Max + Box.V * Box.Pv.Min,
			Box.U * Box.Pu.Max + Box.V * Box.Pv.Max,
			Box.U * Box.Pu.Min + Box.V * Box.Pv.Max
		};
	}

	double MinObbWidth(const TArray<FVector2D>& Polygon)
	{
		const FParcelObb Box = OrientedBoundingBox(Polygon);
		return FMath::Min(Box.Pu.Size, Box.Pv.Size);
	}

	TArray<FVector2D> ClipHalfPlane(const TArray<FVector2D>& Polygon, const FVector2D& Pivot, const FVector2D& Dir, bool bKeepPositive)
	{
		TArray<FVector2D> Out;
		if (Polygon.IsEmpty())
		{
			return Out;
		}

		const auto Side = [&Pivot, &Dir](const FVector2D& Q) { return Cross2D(Dir, Q - Pivot); };

		const int32 N = Polygon.Num();
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = Polygon[i];
			const FVector2D& B = Polygon[(i + 1) % N];
			const double Sa = Side(A);
			const double Sb = Side(B);
			const bool bInA = bKeepPositive ? Sa >= -0.001 : Sa <= 0.001;
			const bool bInB = bKeepPositive ? Sb >= -0.001 : Sb <= 0.001;

			if (bInA && bInB)
			{
				Out.Add(B);
			}
			else if (bInA != bInB)
			{
				const double Denominator = (Sa - Sb) == 0.0 ? 1e-12 : (Sa - Sb);
				const double T = FMath::Clamp(Sa / Denominator, 0.0, 1.0);
				Out.Add(Lerp2D(A, B, T));
				if (!bInA && bInB)
				{
					Out.Add(B);
				}
			}
		}

		return FGrammarGeometry2D::CleanFootprint(Out);
	}

	TPair<TArray<FVector2D>, TArray<FVector2D>> SplitPolygon(const TArray<FVector2D>& Polygon, const FVector2D& Pivot, const FVector2D& Dir)
	{
		return TPair<TArray<FVector2D>, TArray<FVector2D>>(
			ClipHalfPlane(Polygon, Pivot, Dir, true),
			ClipHalfPlane(Polygon, Pivot, Dir, false));
	}

	double CollinearOverlap(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, double Tolerance = 2.5)
	{
		const FVector2D Ab = B - A;
		const double AbLen = Ab.Size();
		if (AbLen < 1e-9)
		{
			return 0.0;
		}
		const FVector2D Axis = Ab / AbLen;
		if (FMath::Max(FMath::Abs(Cross2D(Axis, C - A)), FMath::Abs(Cross2D(Axis, D - A))) > Tolerance)
		{
			return 0.0;
		}
		const double S0 = FVector2D::DotProduct(C - A, Axis);
		const double S1 = FVector2D::DotProduct(D - A, Axis);
		const double Lo = FMath::Max(0.0, FMath::Min(S0, S1));
		const double Hi = FMath::Min(AbLen, FMath::Max(S0, S1));
		return FMath::Max(0.0, Hi - Lo);
	}

	double FrontageLength(const TArray<FVector2D>& Parcel, const TArray<FVector2D>& BlockBoundary)
	{
		double Total = 0.0;
		const int32 PN = Parcel.Num();
		const int32 BN = BlockBoundary.Num();
		for (int32 i = 0; i < PN; ++i)
		{
			const FVector2D& A = Parcel[i];
			const FVector2D& B = Parcel[(i + 1) % PN];
			for (int32 j = 0; j < BN; ++j)
			{
				Total += CollinearOverlap(A, B, BlockBoundary[j], BlockBoundary[(j + 1) % BN]);
			}
		}
		return Total;
	}

	void AnnotateWarnings(TArray<FGrammarParcel>& Parcels, const TArray<FVector2D>& Block, const FGrammarParcelConfig& Config)
	{
		for (FGrammarParcel& P : Parcels)
		{
			P.Frontage = FrontageLength(P.Polygon, Block);
			P.bStreetAccess = P.Frontage > 1.0;
			if (P.Method == TEXT("patio"))
			{
				continue;
			}

			TArray<FString> Parts;
			const double Area = FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(P.Polygon));
			const double Width = MinObbWidth(P.Polygon);
			if (Area < Config.MinArea) Parts.Add(TEXT("area<min"));
			if (Area > Config.MaxArea) Parts.Add(TEXT("area>max"));
			if (Width < Config.MinWidth) Parts.Add(TEXT("width<min"));
			if (!P.bStreetAccess) Parts.Add(TEXT("no street access"));

			P.Warning.Reset();
			for (int32 i = 0; i < Parts.Num(); ++i)
			{
				if (i > 0) P.Warning += TEXT(", ");
				P.Warning += Parts[i];
			}
		}
	}

	bool ShouldSplitObb(const TArray<FVector2D>& Polygon, const TArray<FVector2D>& Block, const FGrammarParcelConfig& Config, int32 Depth)
	{
		if (Depth >= Config.MaxDepth)
		{
			return false;
		}
		const double Area = FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Polygon));
		const double Width = MinObbWidth(Polygon);
		if (Area > Config.MaxArea * 1.08) return true;
		if (Width > Config.MaxWidth * 1.15 && Area > Config.MinArea * 1.35) return true;
		if (FrontageLength(Polygon, Block) <= 1.0 && Area > Config.MinArea * 2.2) return true;

		// Width above only ever checks the OBB's SHORT side against MaxWidth -- a naturally-narrow
		// block that's also very long can have a fine short side and fine total area while still being
		// one long unsplit sliver (see MaxAspectRatio's own comment). The recursive split direction
		// already cuts across the LONG axis by default, so once this triggers, the existing machinery
		// shortens it correctly with no other change needed.
		const FParcelObb Box = OrientedBoundingBox(Polygon);
		const double LongSide = FMath::Max(Box.Pu.Size, Box.Pv.Size);
		const double ShortSide = FMath::Max(1.0, FMath::Min(Box.Pu.Size, Box.Pv.Size));
		if (LongSide / ShortSide > Config.MaxAspectRatio && Area > Config.MinArea * 1.35) return true;
		return false;
	}

	void SubdivideObbRecursive(
		const TArray<FVector2D>& Poly,
		const TArray<FVector2D>& BlockBoundary,
		const FGrammarParcelConfig& Config,
		FRandomStream& Rng,
		int32 BlockId,
		int32 Depth,
		FGrammarParcelSubdivisionResult& Out)
	{
		if (!ShouldSplitObb(Poly, BlockBoundary, Config, Depth))
		{
			FGrammarParcel Parcel;
			Parcel.Id = Out.Parcels.Num();
			Parcel.BlockId = BlockId;
			Parcel.Polygon = FGrammarGeometry2D::OrientFootprintCCW(Poly);
			Parcel.Method = TEXT("obb");
			Parcel.Frontage = FrontageLength(Parcel.Polygon, BlockBoundary);
			Parcel.bStreetAccess = Parcel.Frontage > 1.0;
			Out.Parcels.Add(MoveTemp(Parcel));
			return;
		}

		const FParcelObb Box = OrientedBoundingBox(Poly);
		const bool bLongIsU = Box.Pu.Size >= Box.Pv.Size;
		const FVector2D LongAxis = bLongIsU ? Box.U : Box.V;
		const FVector2D ShortAxis = bLongIsU ? Box.V : Box.U;
		const FParcelProjection LongPr = bLongIsU ? Box.Pu : Box.Pv;
		const FParcelProjection ShortPr = bLongIsU ? Box.Pv : Box.Pu;

		const double MidLong = (LongPr.Min + LongPr.Max) * 0.5;
		const double MidShort = (ShortPr.Min + ShortPr.Max) * 0.5;
		const double Offset = RandomSigned(Rng) * Config.Irregularity * LongPr.Size * 0.275;
		const FVector2D Pivot = LongAxis * (MidLong + Offset) + ShortAxis * MidShort;

		FVector2D SplitDir = ShortAxis;
		TPair<TArray<FVector2D>, TArray<FVector2D>> Split = SplitPolygon(Poly, Pivot, SplitDir);
		bool bValid = Split.Key.Num() >= 3 && Split.Value.Num() >= 3 &&
			FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Split.Key)) > 18.0 &&
			FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Split.Value)) > 18.0 &&
			FGrammarParcelSubdivision::IsSimplePolygon(Split.Key) && FGrammarParcelSubdivision::IsSimplePolygon(Split.Value);

		if (bValid)
		{
			const bool bWeakAccess =
				FrontageLength(Split.Key, BlockBoundary) <= 1.0 ||
				FrontageLength(Split.Value, BlockBoundary) <= 1.0;
			if (bWeakAccess && RandomUniform01(Rng) < Clamp01(Config.StreetAccessPreference))
			{
				TPair<TArray<FVector2D>, TArray<FVector2D>> Rotated = SplitPolygon(Poly, Pivot, LongAxis);
				const bool bRotatedValid = Rotated.Key.Num() >= 3 && Rotated.Value.Num() >= 3 &&
					FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Rotated.Key)) > 18.0 &&
					FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Rotated.Value)) > 18.0 &&
					FGrammarParcelSubdivision::IsSimplePolygon(Rotated.Key) && FGrammarParcelSubdivision::IsSimplePolygon(Rotated.Value);
				if (bRotatedValid)
				{
					SplitDir = LongAxis;
					Split = MoveTemp(Rotated);
				}
			}
		}

		bValid = Split.Key.Num() >= 3 && Split.Value.Num() >= 3 &&
			FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Split.Key)) > 18.0 &&
			FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Split.Value)) > 18.0 &&
			FGrammarParcelSubdivision::IsSimplePolygon(Split.Key) && FGrammarParcelSubdivision::IsSimplePolygon(Split.Value);
		if (!bValid)
		{
			FGrammarParcel Parcel;
			Parcel.Id = Out.Parcels.Num();
			Parcel.BlockId = BlockId;
			Parcel.Polygon = FGrammarGeometry2D::OrientFootprintCCW(Poly);
			Parcel.Method = TEXT("obb");
			Parcel.Warning = TEXT("unsplit");
			Out.Parcels.Add(MoveTemp(Parcel));
			return;
		}

		FGrammarParcelDebugLine Line;
		Line.A = Pivot - SplitDir * 100000.0;
		Line.B = Pivot + SplitDir * 100000.0;
		Line.Kind = TEXT("obb-split");
		Out.Lines.Add(MoveTemp(Line));
		FGrammarParcelDebugPolygon DebugBox;
		DebugBox.Polygon = ObbCorners(Box);
		DebugBox.Kind = TEXT("obb");
		Out.Polygons.Add(MoveTemp(DebugBox));

		TArray<TArray<FVector2D>> Children = {Split.Key, Split.Value};
		Children.Sort([](const TArray<FVector2D>& A, const TArray<FVector2D>& B)
		{
			return FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(A)) > FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(B));
		});

		for (const TArray<FVector2D>& Child : Children)
		{
			SubdivideObbRecursive(FGrammarGeometry2D::OrientFootprintCCW(Child), BlockBoundary, Config, Rng, BlockId, Depth + 1, Out);
		}
	}

	struct FParcelStripStats
	{
		int32 Attempted = 0;
		int32 Accepted = 0;
	};

	// Slices the ring between an outer block contour and an index-corresponded inner contour into
	// frontage quads -- the same operation for both skeleton variants, which only differ in how the
	// inner contour is produced.
	FParcelStripStats GenerateFrontageStrips(
		const TArray<FVector2D>& Block,
		const TArray<FVector2D>& Inner,
		const FGrammarParcelConfig& Config,
		FRandomStream& Rng,
		const FString& Method,
		int32 BlockId,
		FGrammarParcelSubdivisionResult& Out)
	{
		FParcelStripStats Stats;
		const double TargetWidth = (Config.MinWidth + Config.MaxWidth) * 0.5;

		const int32 BN = Block.Num();
		const int32 InnerCount = Inner.Num();
		for (int32 i = 0; i < BN; ++i)
		{
			const FVector2D& A = Block[i];
			const FVector2D& B = Block[(i + 1) % BN];
			const FVector2D& Ia = Inner[i % InnerCount];
			const FVector2D& Ib = Inner[(i + 1) % InnerCount];
			const double EdgeLen = FGrammarGeometry2D::Distance2D(A, B);
			if (EdgeLen < 1e-6)
			{
				continue;
			}

			double T = 0.0;
			int32 Guard = 0;
			while (T < 0.995 && Guard++ < 256)
			{
				const FVector2D P0 = Lerp2D(A, B, T);
				const FVector2D Q0 = Lerp2D(Ia, Ib, T);
				// Local strip depth at this point on the ring -- used only to keep an unusually wide
				// strip from being much wider than it is deep where the block happens to be locally
				// shallow (see MaxAspectRatio's own comment). Never widens a strip, only narrows one
				// that would otherwise come out wide-and-shallow.
				const double LocalDepth = FMath::Max(1.0, FGrammarGeometry2D::Distance2D(P0, Q0));

				const double Jitter = 1.0 + RandomSigned(Rng) * Config.Irregularity * 0.25;
				double Width = FMath::Max(Config.MinWidth * 0.65, FMath::Min(Config.MaxWidth * 1.35, TargetWidth * Jitter));
				Width = FMath::Max(Config.MinWidth * 0.65, FMath::Min(Width, LocalDepth * Config.MaxAspectRatio));
				const double Nt = FMath::Min(1.0, T + Width / EdgeLen);
				const FVector2D P1 = Lerp2D(A, B, Nt);
				const FVector2D Q1 = Lerp2D(Ia, Ib, Nt);
				const TArray<FVector2D> Lot = FGrammarGeometry2D::CleanFootprint({P0, P1, Q1, Q0});
				++Stats.Attempted;

				if (FGrammarParcelSubdivision::IsSimplePolygon(Lot) && FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Lot)) > 18.0)
				{
					FGrammarParcel Parcel;
					Parcel.Id = Out.Parcels.Num();
					Parcel.BlockId = BlockId;
					Parcel.Polygon = FGrammarGeometry2D::OrientFootprintCCW(Lot);
					Parcel.Method = Method;
					Parcel.Frontage = FGrammarGeometry2D::Distance2D(P0, P1);
					Parcel.bStreetAccess = true;
					Out.Parcels.Add(MoveTemp(Parcel));

					FGrammarParcelDebugLine RayLine;
					RayLine.A = P0; RayLine.B = Q0; RayLine.Kind = TEXT("skeleton-ray");
					Out.Lines.Add(MoveTemp(RayLine));
					++Stats.Accepted;
				}
				else
				{
					FGrammarParcelDebugLine RayLine;
					RayLine.A = P0; RayLine.B = Q0; RayLine.Kind = TEXT("skeleton-ray-rejected");
					Out.Lines.Add(MoveTemp(RayLine));
				}
				T = Nt;
			}
		}

		return Stats;
	}

	// IsSimplePolygon only guarantees a single parcel doesn't self-intersect -- it says nothing
	// about two different parcels overlapping each other. Uniform scaling toward one interior
	// anchor can still map closely-spaced concave features (deep, narrow notches) to overlapping
	// inner regions even when every individual quad is simple. This is a cheap proxy: sum the
	// accepted parcel areas and compare against the area the ring is expected to cover.
	bool RingCoverageIsPlausible(const FGrammarParcelSubdivisionResult& Out, double BlockArea, double InnerArea, double Tolerance = 0.15)
	{
		double Sum = 0.0;
		for (const FGrammarParcel& P : Out.Parcels)
		{
			Sum += FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(P.Polygon));
		}
		const double Expected = FMath::Max(1.0, BlockArea - InnerArea);
		const double Ratio = Sum / Expected;
		return Ratio > (1.0 - Tolerance) && Ratio < (1.0 + Tolerance);
	}
}

FGrammarParcelSubdivisionResult FGrammarParcelSubdivision::SubdivideObb(const TArray<FVector2D>& BlockBoundary, const FGrammarParcelConfig& Config, int32 BlockId)
{
	FGrammarParcelSubdivisionResult Out;
	FRandomStream Rng(Config.Seed + BlockId * 9973);
	const TArray<FVector2D> Block = FGrammarGeometry2D::OrientFootprintCCW(BlockBoundary);
	SubdivideObbRecursive(Block, Block, Config, Rng, BlockId, 0, Out);
	AnnotateWarnings(Out.Parcels, Block, Config);
	return Out;
}

FGrammarParcelSubdivisionResult FGrammarParcelSubdivision::SubdivideSkeletonNoOffset(const TArray<FVector2D>& BlockBoundary, const FGrammarParcelConfig& Config, int32 BlockId)
{
	const TArray<FVector2D> Block = FGrammarGeometry2D::OrientFootprintCCW(BlockBoundary);
	FRandomStream Rng(Config.Seed + BlockId * 9973);

	// "No offset" approximates the paper's d_offset -> infinity case: the collapsed inner region
	// shrinks toward the block's interior rather than vanishing to a single point (which would
	// produce triangular wedges instead of roughly-parallel-sided row parcels, and has no
	// containment guarantee on concave blocks).
	const TArray<FVector2D> Inner = SafeInnerContour(Block, /*TargetScale=*/0.15, /*MaxScale=*/0.7, 6);

	if (Inner.IsEmpty())
	{
		FGrammarParcelSubdivisionResult Fallback = SubdivideObb(Block, Config, BlockId);
		for (FGrammarParcel& P : Fallback.Parcels) P.Method = TEXT("skeleton-no-offset-fallback-obb");
		return Fallback;
	}

	FGrammarParcelSubdivisionResult Out;
	const FParcelStripStats Stats = GenerateFrontageStrips(Block, Inner, Config, Rng, TEXT("skeleton-no-offset"), BlockId, Out);

	// Any rejected lot means the strip geometry isn't trustworthy for this block shape. Rather than
	// leave a hole where the rejected lot would have been, fall back to OBB subdivision for the
	// whole block -- OBB splitting always fully partitions its input, so this can't leave gaps.
	if (Stats.Accepted == 0 || Stats.Accepted != Stats.Attempted ||
		!RingCoverageIsPlausible(Out, FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Block)), FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Inner))))
	{
		FGrammarParcelSubdivisionResult Fallback = SubdivideObb(Block, Config, BlockId);
		for (FGrammarParcel& P : Fallback.Parcels) P.Method = TEXT("skeleton-no-offset-fallback-obb");
		return Fallback;
	}

	FGrammarParcelDebugPolygon InnerDebug;
	InnerDebug.Polygon = Inner;
	InnerDebug.Kind = TEXT("collapsed-inner");
	Out.Polygons.Add(MoveTemp(InnerDebug));
	AnnotateWarnings(Out.Parcels, Block, Config);
	return Out;
}

FGrammarParcelSubdivisionResult FGrammarParcelSubdivision::SubdivideSkeletonWithOffset(const TArray<FVector2D>& BlockBoundary, const FGrammarParcelConfig& Config, bool bEmitInnerPatio, int32 BlockId)
{
	const TArray<FVector2D> Block = FGrammarGeometry2D::OrientFootprintCCW(BlockBoundary);
	FRandomStream Rng(Config.Seed + BlockId * 9973);

	// A dependency-free stand-in for a true straight-skeleton offset: blends a depth-driven scale
	// (Config.PerimeterDepth) with Config.OffsetRatio, anchored on a validated interior point, and
	// escalates the scale toward the boundary if the naive blend would self-intersect.
	const FVector2D Anchor = InteriorAnchor(Block);
	const double DepthScale = DepthToScale(Block, Anchor, Config.PerimeterDepth);
	const double RatioScale = Clamp01(Config.OffsetRatio);
	const double BlendedScale = Clamp01((DepthScale + RatioScale) * 0.5);
	const TArray<FVector2D> Inner = SafeInnerContour(Block, BlendedScale, /*MaxScale=*/0.9, 6);

	if (Inner.IsEmpty())
	{
		FGrammarParcelSubdivisionResult Fallback = SubdivideObb(Block, Config, BlockId);
		for (FGrammarParcel& P : Fallback.Parcels) P.Method = TEXT("skeleton-offset-fallback-obb");
		return Fallback;
	}

	FGrammarParcelSubdivisionResult Out;
	const FParcelStripStats Stats = GenerateFrontageStrips(Block, Inner, Config, Rng, TEXT("skeleton-offset"), BlockId, Out);

	if (Stats.Accepted == 0 || Stats.Accepted != Stats.Attempted ||
		!RingCoverageIsPlausible(Out, FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Block)), FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Inner))))
	{
		FGrammarParcelSubdivisionResult Fallback = SubdivideObb(Block, Config, BlockId);
		for (FGrammarParcel& P : Fallback.Parcels) P.Method = TEXT("skeleton-offset-fallback-obb");
		return Fallback;
	}

	FGrammarParcelDebugPolygon InnerDebug;
	InnerDebug.Polygon = Inner;
	InnerDebug.Kind = TEXT("inner-offset");
	Out.Polygons.Add(InnerDebug);

	if (bEmitInnerPatio && IsSimplePolygon(Inner))
	{
		FGrammarParcel Patio;
		Patio.Id = Out.Parcels.Num();
		Patio.BlockId = BlockId;
		Patio.Polygon = Inner;
		Patio.Method = TEXT("patio");
		Patio.Frontage = 0.0;
		Patio.bStreetAccess = false;
		Patio.Warning = TEXT("inner offset / patio");
		Out.Parcels.Add(MoveTemp(Patio));
	}

	AnnotateWarnings(Out.Parcels, Block, Config);
	return Out;
}

FGrammarParcelSubdivisionResult FGrammarParcelSubdivision::SubdivideHybrid(const TArray<FVector2D>& BlockBoundary, const FGrammarParcelConfig& Config, int32 BlockId)
{
	FGrammarParcelSubdivisionResult Out = SubdivideSkeletonWithOffset(BlockBoundary, Config, /*bEmitInnerPatio=*/false, BlockId);

	// If the perimeter pass fell back to OBB (unsafe inner contour on a concave block), it already
	// produced a full, valid subdivision on its own -- there's no separate inner region left to hand
	// to the OBB pass.
	if (Out.Polygons.IsEmpty() || Out.Polygons.Last().Kind != TEXT("inner-offset"))
	{
		return Out;
	}

	const TArray<FVector2D> Inner = Out.Polygons.Last().Polygon;
	if (Inner.Num() >= 3 && FMath::Abs(FGrammarGeometry2D::SignedPolygonArea(Inner)) > Config.MinArea)
	{
		FGrammarParcelConfig InnerConfig = Config;
		InnerConfig.Seed += 431;
		FGrammarParcelSubdivisionResult InnerObb = SubdivideObb(Inner, InnerConfig, BlockId);
		for (FGrammarParcel& P : InnerObb.Parcels)
		{
			P.Id = Out.Parcels.Num();
			P.Method = TEXT("hybrid-inner-obb");
			Out.Parcels.Add(MoveTemp(P));
		}
		Out.Lines.Append(MoveTemp(InnerObb.Lines));
		Out.Polygons.Append(MoveTemp(InnerObb.Polygons));
	}

	return Out;
}

FGrammarParcelSubdivisionResult FGrammarParcelSubdivision::Subdivide(
	const TArray<FVector2D>& BlockBoundary,
	const FGrammarParcelConfig& Config,
	EGrammarParcelSubdivisionMethod Method,
	int32 BlockId)
{
	switch (Method)
	{
	case EGrammarParcelSubdivisionMethod::SkeletonNoOffset:
		return SubdivideSkeletonNoOffset(BlockBoundary, Config, BlockId);
	case EGrammarParcelSubdivisionMethod::SkeletonWithOffset:
		return SubdivideSkeletonWithOffset(BlockBoundary, Config, /*bEmitInnerPatio=*/true, BlockId);
	case EGrammarParcelSubdivisionMethod::Hybrid:
		return SubdivideHybrid(BlockBoundary, Config, BlockId);
	case EGrammarParcelSubdivisionMethod::Obb:
	default:
		return SubdivideObb(BlockBoundary, Config, BlockId);
	}
}
