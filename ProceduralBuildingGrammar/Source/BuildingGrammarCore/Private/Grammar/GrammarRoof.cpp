#include "Grammar/GrammarRoof.h"
#include "Grammar/GrammarRoofDirection.h"
#include "Geometry/GrammarRoofFrame.h"
#include "Geometry/GrammarFace.h"
#include "Geometry/GrammarRoofSkeleton.h"

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
		case EGrammarRoofType::Flat:
		default:
			return FlatRoofMesh(SourceName, Footprint, Height, Roof);
		}
	}
}
