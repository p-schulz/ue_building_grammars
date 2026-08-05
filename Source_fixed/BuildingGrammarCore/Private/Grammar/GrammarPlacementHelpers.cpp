#include "Grammar/GrammarPlacementHelpers.h"
#include "Math/RotationMatrix.h"

FGrammarPlacementRecord FGrammarPlacementHelpers::MakeBoxPlacement(const FString& Role, const FString& VariantKey, const FGrammarBoxPlacementParams& Params, const FLinearColor& Color)
{
	const FVector XAxis(Params.Tangent.X, Params.Tangent.Y, 0.0);
	const FVector YAxis(Params.Normal.X, Params.Normal.Y, 0.0);

	FGrammarPlacementRecord Record;
	Record.Role = Role;
	Record.VariantKey = VariantKey;
	Record.Color = Color;
	Record.Transform.SetLocation(FVector(Params.Center.X, Params.Center.Y, Params.Bottom + Params.Height / 2.0));
	Record.Transform.SetRotation(FRotationMatrix::MakeFromXY(XAxis, YAxis).ToQuat());
	Record.Transform.SetScale3D(FVector(Params.Width, Params.Depth, Params.Height));
	return Record;
}
