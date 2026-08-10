#pragma once

#include "CoreMinimal.h"

// Port of grammar.py's _stable_index -- a deterministic djb2-style rolling hash used to pick a
// stable per-building/per-facade color-variant (or roof-style) index from a string key (typically
// the OSM source/building id). This is NOT FString's built-in hash (which is not guaranteed stable
// across engine versions/platforms the way this simple manual algorithm is) -- reimplement exactly
// as below so regenerating a building always yields the same variant.
class BUILDINGGRAMMARCORE_API FGrammarStableHash
{
public:
	// Returns 0 if Count <= 1. Otherwise walks Value's characters left to right, folding
	// total = (total * 33 + CodePoint) % Count -- must stay in this exact order/base to match.
	static int32 StableIndex(const FString& Value, int32 Count);
};
