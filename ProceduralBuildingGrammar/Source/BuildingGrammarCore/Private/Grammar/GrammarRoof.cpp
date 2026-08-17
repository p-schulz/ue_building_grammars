#include "Grammar/GrammarRoof.h"
#include "Grammar/GrammarRoofDirection.h"
#include "Geometry/GrammarRoofFrame.h"
#include "Geometry/GrammarFace.h"
#include "Geometry/GrammarRoofSkeleton.h"
#include "Geometry/GrammarGeometry2D.h"

namespace
{
	FGrammarMeshSpec MakeRoofMeshSpec(const FString& SourceName, const FRoofStyleConfig& Roof, TArray<FVector> Vertices, TArray<FGrammarFace> Faces)
	{
		FGrammarMeshSpec Mesh;
		Mesh.Name = FString::Printf(TEXT("%s.roof"), *SourceName);
		Mesh.Role = TEXT("roof");
		Mesh.Material = Roof.Material;
		Mesh.Color = Roof.Color;
		Mesh.TexturePath = Roof.TexturePath;
		Mesh.Vertices = MoveTemp(Vertices);
		Mesh.Faces = MoveTemp(Faces);
		Mesh.TextureScale = Roof.TextureScale;
		return Mesh;
	}

	FGrammarMeshSpec PyramidRoofMesh(const FString& SourceName, const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof)
	{
		TArray<FVector> Base = FGrammarRoofFrameMath::RoofBaseVertices(Footprint, Height, Roof.Overhang);

		FVector2D Center = FVector2D::ZeroVector;
		for (const FVector& Point : Base)
		{
			Center += FVector2D(Point.X, Point.Y);
		}
		Center /= FMath::Max(Base.Num(), 1);

		TArray<FVector> Vertices = Base;
		Vertices.Add(FVector(Center.X, Center.Y, Height + Roof.Height));
		const int32 Peak = Vertices.Num() - 1;

		TArray<FGrammarFace> Faces;
		const int32 Count = Base.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Faces.Add(FGrammarFace({ Index, (Index + 1) % Count, Peak }));
		}
		return MakeRoofMeshSpec(SourceName, Roof, MoveTemp(Vertices), MoveTemp(Faces));
	}

	FGrammarMeshSpec GabledRoofMesh(const FString& SourceName, const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags)
	{
		TArray<FVector> Base = FGrammarRoofFrameMath::RoofBaseVertices(Footprint, Height, Roof.Overhang);
		const FGrammarRoofFrame Frame = FGrammarRoofFrameMath::BuildFrame(Base, GrammarRoofDirection::RidgeDirection(Base, Roof, Tags), Height, Height + Roof.Height);

		TArray<FVector> Vertices = Base;
		for (const FVector& Point : Base)
		{
			Vertices.Add(FGrammarRoofFrameMath::RidgeProjection(FVector2D(Point.X, Point.Y), Frame));
		}

		TArray<FGrammarFace> Faces;
		const int32 Count = Base.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 NextIndex = (Index + 1) % Count;
			Faces.Add(FGrammarFace({ Index, NextIndex, Count + NextIndex, Count + Index }));
		}
		return MakeRoofMeshSpec(SourceName, Roof, MoveTemp(Vertices), MoveTemp(Faces));
	}

	// Barn-style: each footprint edge gets TWO stacked quads instead of GabledRoofMesh's one -- a
	// steep lower slope from eave to an intermediate "break" line, then a shallower upper slope from
	// the break line to the same single ridge GabledRoofMesh uses. The break line's position (how far
	// from the ridge, how high up) uses fixed proportions rather than new FRoofStyleConfig fields --
	// same "reasonable hardcoded constant" convention DormerPlacements' own spacing/offset math
	// already uses (GrammarRoofDetails.cpp).
	FGrammarMeshSpec GambrelRoofMesh(const FString& SourceName, const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags)
	{
		constexpr double BreakHeightFraction = 0.65; // fraction of the eave-to-ridge rise where the break line sits
		constexpr double BreakSideFraction = 0.35;   // fraction of the way from ridge (0) to eave where the break line sits

		TArray<FVector> Base = FGrammarRoofFrameMath::RoofBaseVertices(Footprint, Height, Roof.Overhang);
		const FGrammarRoofFrame Frame = FGrammarRoofFrameMath::BuildFrame(Base, GrammarRoofDirection::RidgeDirection(Base, Roof, Tags), Height, Height + Roof.Height);
		const double BreakZ = Frame.EaveZ + (Frame.RidgeZ - Frame.EaveZ) * BreakHeightFraction;

		const int32 Count = Base.Num();
		TArray<FVector> Vertices = Base;
		Vertices.Reserve(Count * 3);
		for (const FVector& Point : Base)
		{
			const FVector2D Point2D(Point.X, Point.Y);
			const double SideValue = FGrammarRoofFrameMath::AxisValue(Point2D, Frame.Center, Frame.Normal);
			const double BreakSide = (SideValue < 0.0 ? Frame.MinSide : Frame.MaxSide) * BreakSideFraction;
			Vertices.Add(FGrammarRoofFrameMath::ProjectAtSideAndHeight(Point2D, Frame, BreakSide, BreakZ));
		}
		for (const FVector& Point : Base)
		{
			Vertices.Add(FGrammarRoofFrameMath::RidgeProjection(FVector2D(Point.X, Point.Y), Frame));
		}

		TArray<FGrammarFace> Faces;
		Faces.Reserve(Count * 2);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 NextIndex = (Index + 1) % Count;
			// Lower (steep) slope: eave -> break line.
			Faces.Add(FGrammarFace({ Index, NextIndex, Count + NextIndex, Count + Index }));
			// Upper (shallow) slope: break line -> ridge.
			Faces.Add(FGrammarFace({ Count + Index, Count + NextIndex, 2 * Count + NextIndex, 2 * Count + Index }));
		}
		return MakeRoofMeshSpec(SourceName, Roof, MoveTemp(Vertices), MoveTemp(Faces));
	}

	// Two stacked hip-roof passes: a steep near-vertical lower band (a straight-skeleton pass capped
	// well before full convergence -- exactly what HippedRoofMesh already produces whenever its own
	// Skeleton.TopRings comes back non-empty, just with a much larger per-XY-unit pitch multiplier so
	// the same recession reads as steep rather than the classic engine's unit-slope hip), then a
	// shallower hip/deck built by re-running the skeleton on the frozen top ring -- a composition
	// that doesn't need any change to FGrammarRoofSkeleton itself, since Build already accepts any
	// simple CCW polygon as input. Fixed proportions (not new FRoofStyleConfig fields), same
	// convention GambrelRoofMesh/DormerPlacements already use.
	FGrammarMeshSpec MansardRoofMesh(const FString& SourceName, const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof)
	{
		constexpr double LowerHeightFraction = 0.6;  // fraction of Roof.Height the steep lower band covers
		constexpr double LowerPitchMultiplier = 3.0; // Z units per XY unit of inward recession -- higher = steeper

		TArray<FVector> Base = FGrammarRoofFrameMath::RoofBaseVertices(Footprint, Height, Roof.Overhang);
		TArray<FVector2D> BaseXY;
		BaseXY.Reserve(Base.Num());
		for (const FVector& Point : Base)
		{
			BaseXY.Add(FVector2D(Point.X, Point.Y));
		}

		const double LowerHeightBudget = Roof.Height * LowerHeightFraction;
		const double LowerCapDistance = LowerHeightBudget / LowerPitchMultiplier;

		FGrammarRoofSkeleton::FResult LowerSkeleton;
		if (!FGrammarRoofSkeleton::Build(BaseXY, LowerCapDistance, LowerSkeleton) || LowerSkeleton.Faces.Num() == 0)
		{
			// Degenerate footprint -- fall back to Pyramid rather than emitting no roof, same as
			// HippedRoofMesh's own fallback.
			return PyramidRoofMesh(SourceName, Footprint, Height, Roof);
		}

		TArray<FVector> Vertices;
		Vertices.Reserve(LowerSkeleton.Nodes.Num());
		for (const FGrammarRoofSkeleton::FNode& Node : LowerSkeleton.Nodes)
		{
			Vertices.Add(FVector(Node.Position.X, Node.Position.Y, Height + Node.Distance * LowerPitchMultiplier));
		}

		TArray<FGrammarFace> Faces;
		Faces.Reserve(LowerSkeleton.Faces.Num() + LowerSkeleton.TopRings.Num());
		for (const FGrammarRoofSkeleton::FFace& Face : LowerSkeleton.Faces)
		{
			Faces.Add(FGrammarFace(Face.NodeIndices));
		}

		if (LowerSkeleton.TopRings.Num() == 0)
		{
			// Fully converged before the cap (small/narrow footprint) -- nothing left to build an
			// upper deck from; the lower pass alone is already a complete (if unusually steep) hip roof.
			return MakeRoofMeshSpec(SourceName, Roof, MoveTemp(Vertices), MoveTemp(Faces));
		}

		const double BreakZ = Height + LowerHeightBudget;
		const double UpperHeightBudget = FMath::Max(Roof.Height - LowerHeightBudget, 0.0);

		for (const TArray<int32>& TopRing : LowerSkeleton.TopRings)
		{
			// The frozen top ring, re-extracted as a fresh 2D polygon and re-prepared exactly like the
			// original footprint (OrientFootprintCCW) before feeding it back into a second Build call.
			TArray<FVector2D> RingXY;
			RingXY.Reserve(TopRing.Num());
			for (const int32 NodeIndex : TopRing)
			{
				RingXY.Add(LowerSkeleton.Nodes[NodeIndex].Position);
			}
			RingXY = FGrammarGeometry2D::OrientFootprintCCW(RingXY);

			FGrammarRoofSkeleton::FResult UpperSkeleton;
			const bool bBuiltUpper = RingXY.Num() >= 3 && UpperHeightBudget > 0.0
				&& FGrammarRoofSkeleton::Build(RingXY, TNumericLimits<double>::Max(), UpperSkeleton)
				&& UpperSkeleton.Nodes.Num() > 0;
			if (!bBuiltUpper)
			{
				// Degenerate ring or no height left to spend -- close it off flat rather than
				// dropping it, same as HippedRoofMesh leaves an unresolved top ring flat.
				Faces.Add(FGrammarFace(TopRing));
				continue;
			}

			double MaxUpperDistance = 0.0;
			for (const FGrammarRoofSkeleton::FNode& Node : UpperSkeleton.Nodes)
			{
				MaxUpperDistance = FMath::Max(MaxUpperDistance, Node.Distance);
			}
			const double UpperPitchMultiplier = MaxUpperDistance > KINDA_SMALL_NUMBER ? UpperHeightBudget / MaxUpperDistance : 0.0;

			// UpperSkeleton was built uncapped (MaxDistance = MAX), so it always fully converges to
			// its own peak(s) -- no TopRings of its own to handle here.
			const int32 IndexOffset = Vertices.Num();
			for (const FGrammarRoofSkeleton::FNode& Node : UpperSkeleton.Nodes)
			{
				Vertices.Add(FVector(Node.Position.X, Node.Position.Y, BreakZ + Node.Distance * UpperPitchMultiplier));
			}
			for (const FGrammarRoofSkeleton::FFace& Face : UpperSkeleton.Faces)
			{
				TArray<int32> OffsetIndices;
				OffsetIndices.Reserve(Face.NodeIndices.Num());
				for (const int32 NodeIndex : Face.NodeIndices)
				{
					OffsetIndices.Add(NodeIndex + IndexOffset);
				}
				Faces.Add(FGrammarFace(MoveTemp(OffsetIndices)));
			}
		}

		return MakeRoofMeshSpec(SourceName, Roof, MoveTemp(Vertices), MoveTemp(Faces));
	}

	// Straight-skeleton hip roof (FGrammarRoofSkeleton) -- a true frustum: every footprint edge
	// recedes inward at unit speed until it either converges to a ridge/apex point or is capped at
	// Roof.Height (whichever comes first for that part of the footprint), robustly handling
	// non-convex (L/T/U-shaped) footprints via the skeleton's own split-event machinery. Replaced
	// the previous single-ridge-line approximation (still used by GabledRoofMesh, which needs a
	// straight ridge with vertical gable-end walls -- not what a plain straight skeleton produces).
	// Tags is unused here (no ridge-direction concept applies -- the skeleton's shape is entirely
	// determined by the footprint itself) but kept in the signature for a uniform dispatch call.
	FGrammarMeshSpec HippedRoofMesh(const FString& SourceName, const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags)
	{
		(void)Tags;

		TArray<FVector> Base = FGrammarRoofFrameMath::RoofBaseVertices(Footprint, Height, Roof.Overhang);
		TArray<FVector2D> BaseXY;
		BaseXY.Reserve(Base.Num());
		for (const FVector& Point : Base)
		{
			BaseXY.Add(FVector2D(Point.X, Point.Y));
		}

		FGrammarRoofSkeleton::FResult Skeleton;
		if (!FGrammarRoofSkeleton::Build(BaseXY, Roof.Height, Skeleton) || Skeleton.Faces.Num() == 0)
		{
			// Degenerate footprint (skeleton couldn't be built at all) -- fall back to Pyramid
			// rather than emitting no roof.
			return PyramidRoofMesh(SourceName, Footprint, Height, Roof);
		}

		TArray<FVector> Vertices;
		Vertices.Reserve(Skeleton.Nodes.Num());
		for (const FGrammarRoofSkeleton::FNode& Node : Skeleton.Nodes)
		{
			Vertices.Add(FVector(Node.Position.X, Node.Position.Y, Height + Node.Distance));
		}

		TArray<FGrammarFace> Faces;
		Faces.Reserve(Skeleton.Faces.Num() + Skeleton.TopRings.Num());
		for (const FGrammarRoofSkeleton::FFace& Face : Skeleton.Faces)
		{
			Faces.Add(FGrammarFace(Face.NodeIndices));
		}
		for (const TArray<int32>& TopRing : Skeleton.TopRings)
		{
			Faces.Add(FGrammarFace(TopRing));
		}
		return MakeRoofMeshSpec(SourceName, Roof, MoveTemp(Vertices), MoveTemp(Faces));
	}

	FGrammarMeshSpec FlatRoofMesh(const FString& SourceName, const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof)
	{
		const double RoofZ = Roof.bEdgeEnabled ? Height - FMath::Max(Roof.SurfaceInset, 0.0) : Height;
		TArray<FVector> Vertices = FGrammarRoofFrameMath::RoofBaseVertices(Footprint, RoofZ, Roof.Overhang);

		TArray<int32> Indices;
		Indices.Reserve(Vertices.Num());
		for (int32 Index = 0; Index < Vertices.Num(); ++Index)
		{
			Indices.Add(Index);
		}
		TArray<FGrammarFace> Faces = { FGrammarFace(MoveTemp(Indices)) };
		return MakeRoofMeshSpec(SourceName, Roof, MoveTemp(Vertices), MoveTemp(Faces));
	}
}

namespace GrammarRoof
{
	FGrammarMeshSpec RoofMesh(const FString& SourceName, const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags)
	{
		switch (Roof.Type)
		{
		case EGrammarRoofType::Gabled:
			return GabledRoofMesh(SourceName, Footprint, Height, Roof, Tags);
		case EGrammarRoofType::Hipped:
			return HippedRoofMesh(SourceName, Footprint, Height, Roof, Tags);
		case EGrammarRoofType::Pyramid:
			return PyramidRoofMesh(SourceName, Footprint, Height, Roof);
		case EGrammarRoofType::Gambrel:
			return GambrelRoofMesh(SourceName, Footprint, Height, Roof, Tags);
		case EGrammarRoofType::Mansard:
			return MansardRoofMesh(SourceName, Footprint, Height, Roof);
		case EGrammarRoofType::Flat:
		default:
			return FlatRoofMesh(SourceName, Footprint, Height, Roof);
		}
	}
}
