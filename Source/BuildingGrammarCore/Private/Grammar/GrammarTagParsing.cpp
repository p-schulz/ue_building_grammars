#include "Grammar/GrammarTagParsing.h"

namespace
{
	bool LooksNumeric(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		bool bHasDigit = false;
		for (const TCHAR Ch : Value)
		{
			if (FChar::IsDigit(Ch))
			{
				bHasDigit = true;
				continue;
			}
			if (Ch == TEXT('.') || Ch == TEXT('-') || Ch == TEXT('+') || Ch == TEXT('e') || Ch == TEXT('E'))
			{
				continue;
			}
			return false;
		}
		return bHasDigit;
	}
}

TOptional<double> FGrammarTagParsing::ParseMeters(const FString* Value)
{
	if (!Value || Value->IsEmpty())
	{
		return TOptional<double>();
	}
	FString Normalized = Value->TrimStartAndEnd().ToLower();
	Normalized = Normalized.Replace(TEXT("meters"), TEXT("m")).Replace(TEXT("metres"), TEXT("m"));
	if (Normalized.EndsWith(TEXT("m")))
	{
		Normalized = Normalized.LeftChop(1).TrimStartAndEnd();
	}
	if (!LooksNumeric(Normalized))
	{
		return TOptional<double>();
	}
	return FCString::Atod(*Normalized);
}

TOptional<int32> FGrammarTagParsing::ParseIntTag(const FString* Value)
{
	if (!Value || Value->IsEmpty())
	{
		return TOptional<int32>();
	}
	const FString Trimmed = Value->TrimStartAndEnd();
	if (!LooksNumeric(Trimmed))
	{
		return TOptional<int32>();
	}
	return static_cast<int32>(FCString::Atod(*Trimmed));
}
