#include "Geometry/GrammarMeshUVs.h"

void FGrammarMeshUVs::PickUVAxes(const TArray<FVector>& FaceVertices, FVector& OutAxisA, FVector& OutAxisB)
{
	if (FaceVertices.Num() < 3)
	{
		OutAxisA = FVector(1.0, 0.0, 0.0);
		OutAxisB = FVector(0.0, 1.0, 0.0);
		return;
	}
	const FVector AB = FaceVertices[1] - FaceVertices[0];
	const FVector AC = FaceVertices[2] - FaceVertices[0];
	const FVector Normal = FVector::CrossProduct(AB, AC).GetSafeNormal();
	const FVector AbsNormal(FMath::Abs(Normal.X), FMath::Abs(Normal.Y), FMath::Abs(Normal.Z));

	if (AbsNormal.Z >= AbsNormal.X && AbsNormal.Z >= AbsNormal.Y)
	{
		OutAxisA = FVector(1.0, 0.0, 0.0);
		OutAxisB = FVector(0.0, 1.0, 0.0);
	}
	else if (AbsNormal.X >= AbsNormal.Y)
	{
		OutAxisA = FVector(0.0, 1.0, 0.0);
		OutAxisB = FVector(0.0, 0.0, 1.0);
	}
	else
	{
		OutAxisA = FVector(1.0, 0.0, 0.0);
		OutAxisB = FVector(0.0, 0.0, 1.0);
	}
}

TArray<FVector2D> FGrammarMeshUVs::ComputeFaceUVs(const TArray<FVector>& Vertices, const FGrammarFace& Face, double TextureScale)
{
	TArray<FVector2D> Result;
	if (Face.Indices.Num() == 0)
	{
		return Result;
	}

	TArray<FVector> FaceVertices;
	FaceVertices.Reserve(Face.Indices.Num());
	for (const int32 Index : Face.Indices)
	{
		FaceVertices.Add(Vertices[Index]);
	}

	FVector AxisA, AxisB;
	PickUVAxes(FaceVertices, AxisA, AxisB);

	const double Scale = FMath::Max(TextureScale, 0.001);
	const FVector Origin = FaceVertices[0];

	Result.Reserve(FaceVertices.Num());
	for (const FVector& Vertex : FaceVertices)
	{
		const FVector Delta = Vertex - Origin;
		Result.Add(FVector2D(FVector::DotProduct(Delta, AxisA) / Scale, FVector::DotProduct(Delta, AxisB) / Scale));
	}
	return Result;
}
