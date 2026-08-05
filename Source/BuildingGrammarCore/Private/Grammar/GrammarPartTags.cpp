#include "Grammar/GrammarPartTags.h"
#include "Grammar/GrammarTagParsing.h"

namespace
{
	TOptional<double> ParseMetersEither(const TMap<FString, FString>& Tags, const TCHAR* PrimaryKey, const TCHAR* FallbackKey)
	{
		TOptional<double> Value = FGrammarTagParsing::ParseMeters(Tags.Find(PrimaryKey));
		if (!Value.IsSet())
		{
			Value = FGrammarTagParsing::ParseMeters(Tags.Find(FallbackKey));
		}
		return Value;
	}

	TOptional<int32> ParseIntEither(const TMap<FString, FString>& Tags, const TCHAR* PrimaryKey, const TCHAR* FallbackKey)
	{
		TOptional<int32> Value = FGrammarTagParsing::ParseIntTag(Tags.Find(PrimaryKey));
		if (!Value.IsSet())
		{
			Value = FGrammarTagParsing::ParseIntTag(Tags.Find(FallbackKey));
		}
		return Value;
	}
}

double FGrammarPartTags::EffectivePartMinHeight(const TMap<FString, FString>& Tags, const FBuildingGrammarConfig& Config)
{
	const TOptional<double> Explicit = ParseMetersEither(Tags, TEXT("min_height"), TEXT("building:min_height"));
	if (Explicit.IsSet())
	{
		return FMath::Max(Explicit.GetValue(), 0.0);
	}

	const TOptional<int32> MinLevel = ParseIntEither(Tags, TEXT("building:min_level"), TEXT("min_level"));
	if (!MinLevel.IsSet())
	{
		return 0.0;
	}
	return FMath::Max(MinLevel.GetValue(), 0) * Config.DefaultFloorHeight;
}

TPair<TMap<FString, FString>, double> FGrammarPartTags::TagsForBuildingPartVolume(const TMap<FString, FString>& Tags, const FBuildingGrammarConfig& Config)
{
	const double MinHeight = EffectivePartMinHeight(Tags, Config);
	if (MinHeight <= 0.0)
	{
		return TPair<TMap<FString, FString>, double>(Tags, 0.0);
	}

	TMap<FString, FString> VolumeTags = Tags;

	const TOptional<double> ExplicitHeight = FGrammarTagParsing::ParseMeters(VolumeTags.Find(TEXT("height")));
	if (ExplicitHeight.IsSet())
	{
		VolumeTags.Add(TEXT("grammar:part:absolute_height"), FString::Printf(TEXT("%.6f"), ExplicitHeight.GetValue()));
		VolumeTags.Add(TEXT("height"), FString::Printf(TEXT("%.6f"), FMath::Max(ExplicitHeight.GetValue() - MinHeight, 0.1)));
	}
	else
	{
		const TOptional<int32> Levels = ParseIntEither(VolumeTags, TEXT("building:levels"), TEXT("levels"));
		const TOptional<int32> MinLevel = ParseIntEither(VolumeTags, TEXT("building:min_level"), TEXT("min_level"));
		if (Levels.IsSet() && MinLevel.IsSet())
		{
			const int32 EffectiveLevels = FMath::Max(Levels.GetValue() - FMath::Max(MinLevel.GetValue(), 0), 1);
			VolumeTags.Add(TEXT("building:levels"), FString::FromInt(EffectiveLevels));
			VolumeTags.Remove(TEXT("levels"));
		}
	}

	VolumeTags.Add(TEXT("grammar:part:min_height"), FString::Printf(TEXT("%.6f"), MinHeight));
	VolumeTags.Add(TEXT("grammar:disable_ground_entrance"), TEXT("yes"));

	return TPair<TMap<FString, FString>, double>(MoveTemp(VolumeTags), MinHeight);
}
