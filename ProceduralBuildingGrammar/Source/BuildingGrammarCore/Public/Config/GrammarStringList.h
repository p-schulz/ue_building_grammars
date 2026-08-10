#pragma once

#include "CoreMinimal.h"
#include "GrammarStringList.generated.h"

// UHT does not allow a TMap value to itself be a container (e.g. TMap<FString, TArray<FString>>
// is not legal UPROPERTY reflection), so tag_filters' dict[str, list[str]] from config.py's
// FacadeStyleConfig needs its TArray<FString> wrapped in a one-field USTRUCT to be a valid map
// value type.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarStringList
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	TArray<FString> Values;
};
