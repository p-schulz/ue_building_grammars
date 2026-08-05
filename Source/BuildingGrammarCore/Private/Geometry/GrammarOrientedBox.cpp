#include "Geometry/GrammarOrientedBox.h"

void FGrammarOrientedBox::Build(
	const FVector2D& Center,
	const FVector2D& Tangent,
	const FVector2D& Normal,
	double Width,
	double Depth,
	double Height,
	double Bottom,
	TArray<FVector>& OutVertices,
	TArray<FGrammarFace>& OutFaces)
{
	const double HalfWidth = Width / 2.0;
	const double HalfDepth = Depth / 2.0;

	FVector2D Corners[4];
	const double Laterals[4] = { -HalfWidth, HalfWidth, HalfWidth, -HalfWidth };
	const double Outwards[4] = { -HalfDepth, -HalfDepth, HalfDepth, HalfDepth };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Corners[Index] = Center + Tangent * Laterals[Index] + Normal * Outwards[Index];
	}

	OutVertices.Reset(8);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		OutVertices.Add(FVector(Corners[Index].X, Corners[Index].Y, Bottom));
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		OutVertices.Add(FVector(Corners[Index].X, Corners[Index].Y, Bottom + Height));
	}

	OutFaces.Reset(6);
	OutFaces.Add(FGrammarFace({ 0, 1, 2, 3 }));
	OutFaces.Add(FGrammarFace({ 4, 7, 6, 5 }));
	OutFaces.Add(FGrammarFace({ 0, 4, 5, 1 }));
	OutFaces.Add(FGrammarFace({ 1, 5, 6, 2 }));
	OutFaces.Add(FGrammarFace({ 2, 6, 7, 3 }));
	OutFaces.Add(FGrammarFace({ 3, 7, 4, 0 }));
}
