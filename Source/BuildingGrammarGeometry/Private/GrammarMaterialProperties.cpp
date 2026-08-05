#include "GrammarMaterialProperties.h"
#include <initializer_list>

namespace
{
	bool ContainsAny(const FString& Lower, std::initializer_list<const TCHAR*> Keywords)
	{
		for (const TCHAR* Keyword : Keywords)
		{
			if (Lower.Contains(Keyword))
			{
				return true;
			}
		}
		return false;
	}

	bool IsMetalFamily(const FString& Lower)
	{
		return ContainsAny(Lower, { TEXT("metal"), TEXT("steel"), TEXT("aluminum"), TEXT("aluminium"), TEXT("zinc"), TEXT("copper") });
	}
}

float FGrammarMaterialProperties::RoughnessForMaterialName(const FString& MaterialName)
{
	const FString Lower = MaterialName.ToLower();
	if (ContainsAny(Lower, { TEXT("glass"), TEXT("window"), TEXT("pv"), TEXT("solar") }))
	{
		return 0.18f;
	}
	if (IsMetalFamily(Lower))
	{
		return 0.32f;
	}
	if (ContainsAny(Lower, { TEXT("roof"), TEXT("tile"), TEXT("brick"), TEXT("concrete"), TEXT("stone"), TEXT("render"), TEXT("plaster") }))
	{
		return 0.72f;
	}
	return 0.58f;
}

float FGrammarMaterialProperties::MetallicForMaterialName(const FString& MaterialName)
{
	return IsMetalFamily(MaterialName.ToLower()) ? 0.65f : 0.0f;
}
