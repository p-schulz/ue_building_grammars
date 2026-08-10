#pragma once

#include "CoreMinimal.h"
#include "LedgeStyleConfig.generated.h"

// Port of config.py's LedgeStyleConfig. Defaults match the Python dataclass exactly.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FLedgeStyleConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge")
	double Depth = 0.16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge")
	double Height = 0.08;

	// A ledge is placed on every floor whose index is a multiple of this value; 0 disables ledges.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge")
	int32 EveryNFloors = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge")
	FString Material = TEXT("Grammar Ledges");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge")
	FLinearColor Color = FLinearColor(0.78, 0.74, 0.68, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge")
	FString TexturePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge")
	double TextureScale = 1.0;
};
