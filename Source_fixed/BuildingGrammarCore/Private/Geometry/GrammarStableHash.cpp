#include "Geometry/GrammarStableHash.h"

int32 FGrammarStableHash::StableIndex(const FString& Value, int32 Count)
{
	if (Count <= 1)
	{
		return 0;
	}
	int64 Total = 0;
	for (const TCHAR Char : Value)
	{
		Total = (Total * 33 + static_cast<int64>(Char)) % static_cast<int64>(Count);
	}
	return static_cast<int32>(Total);
}
