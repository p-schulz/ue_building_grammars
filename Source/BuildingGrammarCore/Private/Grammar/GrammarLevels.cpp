#include "Grammar/GrammarLevels.h"
#include "Grammar/GrammarTagParsing.h"

int32 FGrammarLevels::InferLevels(const TMap<FString, FString>& Tags, const FBuildingGrammarConfig& Config, const FFacadeStyleConfig* Style)
{
	for (const TCHAR* Key : { TEXT("building:levels"), TEXT("levels") })
	{
		if (const TOptional<int32> Value = FGrammarTagParsing::ParseIntTag(Tags.Find(Key)))
		{
			return FMath::Max(Value.GetValue(), 1);
		}
	}

	const TOptional<double> Height = FGrammarTagParsing::ParseMeters(Tags.Find(TEXT("height")));
	const double FloorHeight = (Style && Style->bHasDefaultFloorHeight) ? Style->DefaultFloorHeight : Config.DefaultFloorHeight;
	if (Height.IsSet())
	{
		return FMath::Max(1, FMath::RoundToInt32(Height.GetValue() / FloorHeight));
	}
	if (Style && Style->bHasDefaultLevels)
	{
		return FMath::Max(Style->DefaultLevels, 1);
	}
	return FMath::Max(Config.DefaultLevels, 1);
}

TArray<double> FGrammarLevels::FloorHeightSequence(int32 Levels, const TMap<FString, FString>& Tags, const FBuildingGrammarConfig& Config, const FFacadeStyleConfig* Style)
{
	const TOptional<double> ExplicitHeight = FGrammarTagParsing::ParseMeters(Tags.Find(TEXT("height")));
	const double DefaultFloorHeight = (Style && Style->bHasDefaultFloorHeight) ? Style->DefaultFloorHeight : Config.DefaultFloorHeight;

	TArray<double> Heights;
	Heights.Reserve(Levels);
	for (int32 Index = 0; Index < Levels; ++Index)
	{
		const double* Irregular = Config.IrregularFloorHeights.Find(Index);
		Heights.Add(Irregular ? *Irregular : DefaultFloorHeight);
	}

	if (ExplicitHeight.IsSet())
	{
		double Sum = 0.0;
		for (const double Height : Heights)
		{
			Sum += Height;
		}
		if (Sum > 0.0)
		{
			const double Scale = ExplicitHeight.GetValue() / Sum;
			for (double& Height : Heights)
			{
				Height *= Scale;
			}
		}
	}

	return Heights;
}
