#include "Grammar/GrammarStyleSelection.h"

namespace
{
	FString GetTrimmedLower(const TMap<FString, FString>& Tags, const TCHAR* Key)
	{
		const FString* Value = Tags.Find(Key);
		return Value ? Value->TrimStartAndEnd().ToLower() : FString();
	}
}

bool FGrammarStyleSelection::BuildingValueIsExcluded(const TMap<FString, FString>& Tags, const TArray<FString>& ExcludedBuildingValues, const TArray<FFacadeStyleConfig>& Styles)
{
	const FString* BuildingValue = Tags.Find(TEXT("building"));
	if (!BuildingValue)
	{
		return false;
	}
	TSet<FString> Excluded;
	for (const FString& Value : ExcludedBuildingValues)
	{
		Excluded.Add(Value.TrimStartAndEnd().ToLower());
	}
	if (!Excluded.Contains(BuildingValue->TrimStartAndEnd().ToLower()))
	{
		return false;
	}
	return MatchingStylesForTags(Tags, Styles).Num() == 0;
}

TArray<const FFacadeStyleConfig*> FGrammarStyleSelection::MatchingStylesForTags(const TMap<FString, FString>& Tags, const TArray<FFacadeStyleConfig>& Styles)
{
	TArray<const FFacadeStyleConfig*> Result;
	for (const FFacadeStyleConfig& Style : Styles)
	{
		if (StyleMatchesTags(Style, Tags))
		{
			Result.Add(&Style);
		}
	}
	return Result;
}

bool FGrammarStyleSelection::StyleMatchesTags(const FFacadeStyleConfig& Style, const TMap<FString, FString>& Tags)
{
	if (Style.BuildingValues.Num() > 0)
	{
		TSet<FString> Normalized;
		for (const TCHAR* Key : { TEXT("building"), TEXT("building:part"), TEXT("building:use") })
		{
			const FString Value = GetTrimmedLower(Tags, Key);
			if (!Value.IsEmpty())
			{
				Normalized.Add(Value);
			}
		}
		for (const FString& Allowed : Style.BuildingValues)
		{
			if (Normalized.Contains(Allowed.TrimStartAndEnd().ToLower()))
			{
				return true;
			}
		}
	}

	for (const TPair<FString, FGrammarStringList>& Filter : Style.TagFilters)
	{
		const FString* TagValue = Tags.Find(Filter.Key);
		if (!TagValue)
		{
			continue;
		}
		const FString NormalizedTagValue = TagValue->TrimStartAndEnd().ToLower();
		for (const FString& Allowed : Filter.Value.Values)
		{
			const FString NormalizedAllowed = Allowed.TrimStartAndEnd().ToLower();
			if (NormalizedAllowed == NormalizedTagValue || NormalizedAllowed == TEXT("*"))
			{
				return true;
			}
		}
	}
	return false;
}

TArray<const FFacadeStyleConfig*> FGrammarStyleSelection::SemanticStylesForTags(const TMap<FString, FString>& Tags, const TArray<FFacadeStyleConfig>& Styles)
{
	const TMap<FString, int32> Keywords = SemanticStyleKeywords(Tags);
	if (Keywords.Num() == 0)
	{
		return {};
	}

	struct FScoredStyle
	{
		int32 Score;
		int32 Index;
		const FFacadeStyleConfig* Style;
	};

	TArray<FScoredStyle> Scored;
	for (int32 Index = 0; Index < Styles.Num(); ++Index)
	{
		const FString Haystack = (Styles[Index].Name + TEXT(" ") + Styles[Index].WallMaterial).ToLower();
		int32 Score = 0;
		for (const TPair<FString, int32>& Keyword : Keywords)
		{
			if (Haystack.Contains(Keyword.Key))
			{
				Score += Keyword.Value;
			}
		}
		if (Score > 0)
		{
			Scored.Add({ Score, Index, &Styles[Index] });
		}
	}

	// Descending score; ties keep ascending original-index order (mirrors Python's
	// scored.sort(reverse=True) on (score, -index, style) tuples).
	Scored.Sort([](const FScoredStyle& A, const FScoredStyle& B)
	{
		return A.Score != B.Score ? A.Score > B.Score : A.Index < B.Index;
	});

	TArray<const FFacadeStyleConfig*> Result;
	Result.Reserve(Scored.Num());
	for (const FScoredStyle& Entry : Scored)
	{
		Result.Add(Entry.Style);
	}
	return Result;
}

TMap<FString, int32> FGrammarStyleSelection::SemanticStyleKeywords(const TMap<FString, FString>& Tags)
{
	TMap<FString, int32> Keywords;

	const FString Building = GetTrimmedLower(Tags, TEXT("building"));
	const FString BuildingUse = GetTrimmedLower(Tags, TEXT("building:use"));
	const FString Shop = GetTrimmedLower(Tags, TEXT("shop"));
	const FString Office = GetTrimmedLower(Tags, TEXT("office"));
	const FString Industrial = GetTrimmedLower(Tags, TEXT("industrial"));
	const FString Landuse = GetTrimmedLower(Tags, TEXT("landuse"));
	const FString Amenity = GetTrimmedLower(Tags, TEXT("amenity"));
	const FString Religion = GetTrimmedLower(Tags, TEXT("religion"));

	if (!Shop.IsEmpty())
	{
		Keywords.Add(TEXT("retail"), 4);
		Keywords.Add(TEXT("shop"), 4);
		if (Shop == TEXT("supermarket") || Building == TEXT("supermarket"))
		{
			Keywords.Add(TEXT("supermarket"), 8);
		}
	}
	if (!Office.IsEmpty() || Building == TEXT("office") || Building == TEXT("commercial") || BuildingUse == TEXT("office"))
	{
		Keywords.Add(TEXT("office"), 7);
		Keywords.Add(TEXT("curtain"), 2);
		Keywords.Add(TEXT("atrium"), 2);
		Keywords.Add(TEXT("steel"), 1);
	}
	if (!Industrial.IsEmpty() || Landuse == TEXT("industrial")
		|| Building == TEXT("industrial") || Building == TEXT("warehouse") || Building == TEXT("factory") || Building == TEXT("manufacture"))
	{
		Keywords.Add(TEXT("industrial"), 8);
		Keywords.Add(TEXT("warehouse"), 6);
		Keywords.Add(TEXT("factory"), 4);
	}
	if (Building == TEXT("church") || Building == TEXT("cathedral") || Building == TEXT("chapel") || Building == TEXT("religious") || Amenity == TEXT("place_of_worship"))
	{
		Keywords.Add(TEXT("church"), 8);
		Keywords.Add(TEXT("cathedral"), 6);
		Keywords.Add(TEXT("sacral"), 5);
		Keywords.Add(TEXT("stone"), 2);
		Keywords.Add(TEXT("historic"), 2);
		const FString Name = GetTrimmedLower(Tags, TEXT("name"));
		if (Building == TEXT("cathedral") || Name.Contains(TEXT("cathedral")))
		{
			Keywords.Add(TEXT("cathedral"), 10);
		}
		if (!Religion.IsEmpty())
		{
			Keywords.Add(Religion, 2);
		}
	}

	FString StartDate = GetTrimmedLower(Tags, TEXT("start_date"));
	if (StartDate.IsEmpty())
	{
		StartDate = GetTrimmedLower(Tags, TEXT("building:start_date"));
	}
	if (const TOptional<int32> Year = StartYear(StartDate))
	{
		const int32 YearValue = Year.GetValue();
		if (YearValue < 1918)
		{
			Keywords.Add(TEXT("gruenderzeit"), 4);
			Keywords.Add(TEXT("jugendstil"), 3);
			Keywords.Add(TEXT("fachwerk"), 2);
			Keywords.Add(TEXT("historic"), 2);
		}
		else if (YearValue < 1935)
		{
			Keywords.Add(TEXT("bauhaus"), 5);
			Keywords.Add(TEXT("siedlung"), 4);
		}
		else if (YearValue < 1975)
		{
			Keywords.Add(TEXT("postwar"), 4);
			Keywords.Add(TEXT("nachkriegsmoderne"), 4);
		}
		else if (YearValue < 1995)
		{
			Keywords.Add(TEXT("plattenbau"), 5);
			Keywords.Add(TEXT("prefab"), 3);
		}
		else
		{
			Keywords.Add(TEXT("contemporary"), 4);
			Keywords.Add(TEXT("modern"), 3);
			Keywords.Add(TEXT("passivhaus"), 2);
		}
	}

	return Keywords;
}

TOptional<int32> FGrammarStyleSelection::StartYear(const FString& Value)
{
	if (Value.IsEmpty())
	{
		return TOptional<int32>();
	}
	FString FirstDigitRun;
	bool bInRun = false;
	for (const TCHAR Ch : Value)
	{
		if (FChar::IsDigit(Ch))
		{
			FirstDigitRun.AppendChar(Ch);
			bInRun = true;
		}
		else if (bInRun)
		{
			break;
		}
	}
	if (FirstDigitRun.IsEmpty())
	{
		return TOptional<int32>();
	}
	const int32 Year = FCString::Atoi(*FirstDigitRun.Left(4));
	if (Year < 1000 || Year > 2200)
	{
		return TOptional<int32>();
	}
	return Year;
}

TArray<const FFacadeStyleConfig*> FGrammarStyleSelection::SelectableStylesForTags(const TMap<FString, FString>& Tags, const TArray<FFacadeStyleConfig>& Styles)
{
	TArray<const FFacadeStyleConfig*> Matched = MatchingStylesForTags(Tags, Styles);
	if (Matched.Num() > 0)
	{
		return Matched;
	}
	TArray<const FFacadeStyleConfig*> Semantic = SemanticStylesForTags(Tags, Styles);
	if (Semantic.Num() > 0)
	{
		return Semantic;
	}
	TArray<const FFacadeStyleConfig*> Unfiltered;
	for (const FFacadeStyleConfig& Style : Styles)
	{
		if (Style.BuildingValues.Num() == 0 && Style.TagFilters.Num() == 0)
		{
			Unfiltered.Add(&Style);
		}
	}
	if (Unfiltered.Num() > 0)
	{
		return Unfiltered;
	}
	TArray<const FFacadeStyleConfig*> All;
	All.Reserve(Styles.Num());
	for (const FFacadeStyleConfig& Style : Styles)
	{
		All.Add(&Style);
	}
	return All;
}
