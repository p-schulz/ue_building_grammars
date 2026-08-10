#include "Grammar/GrammarRoof.h"
#include "Grammar/GrammarRoofDirection.h"
#include "Geometry/GrammarRoofFrame.h"
#include "Geometry/GrammarFace.h"

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

	FGrammarMeshSpec HippedRoofMesh(const FString& SourceName, const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags)
	{
		TArray<FVector> Base = FGrammarRoofFrameMath::RoofBaseVertices(Footprint, Height, Roof.Overhang);
		const FGrammarRoofFrame Frame = FGrammarRoofFrameMath::BuildFrame(Base, GrammarRoofDirection::RidgeDirection(Base, Roof, Tags), Height, Height + Roof.Height);

		const double LongSpan = Frame.MaxLong - Frame.MinLong;
		const double SideSpan = Frame.MaxSide - Frame.MinSide;
		if (LongSpan <= SideSpan * 1.25)
		{
			return PyramidRoofMesh(SourceName, Footprint, Height, Roof);
		}

		const double Inset = FMath::Min(SideSpan * 0.42, LongSpan * 0.24);
		const FVector RidgeStart = FGrammarRoofFrameMath::PointFromRoofAxes(Frame, Frame.MinLong + Inset, 0.0, Frame.RidgeZ);
		const FVector RidgeEnd = FGrammarRoofFrameMath::PointFromRoofAxes(Frame, Frame.MaxLong - Inset, 0.0, Frame.RidgeZ);

		TArray<FVector> Vertices = Base;
		Vertices.Add(RidgeStart);
		Vertices.Add(RidgeEnd);
		const int32 RidgeStartIndex = Base.Num();
		const int32 RidgeEndIndex = Base.Num() + 1;

		TArray<FGrammarFace> Faces;
		const int32 Count = Base.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 NextIndex = (Index + 1) % Count;
			const double Station = FGrammarRoofFrameMath::AxisValue(FVector2D(Base[Index].X, Base[Index].Y), Frame.Center, Frame.Direction);
			const double NextStation = FGrammarRoofFrameMath::AxisValue(FVector2D(Base[NextIndex].X, Base[NextIndex].Y), Frame.Center, Frame.Direction);

			if (Station < Frame.MinLong + Inset && NextStation < Frame.MinLong + Inset)
			{
				Faces.Add(FGrammarFace({ Index, NextIndex, RidgeStartIndex }));
			}
			else if (Station > Frame.MaxLong - Inset && NextStation > Frame.MaxLong - Inset)
			{
				Faces.Add(FGrammarFace({ Index, NextIndex, RidgeEndIndex }));
			}
			else
			{
				Faces.Add(FGrammarFace({ Index, NextIndex, RidgeEndIndex, RidgeStartIndex }));
			}
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
