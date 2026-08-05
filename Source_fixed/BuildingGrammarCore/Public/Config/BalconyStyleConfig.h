#pragma once

#include "CoreMinimal.h"
#include "BalconyStyleConfig.generated.h"

// Port of config.py's BalconyStyleConfig. Defaults match the Python dataclass exactly.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FBalconyStyleConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony")
	double Width = 1.9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony")
	double Depth = 0.75;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony")
	double SlabHeight = 0.12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony")
	double RailingHeight = 0.9;

	// A balcony is placed on every floor whose index is a positive multiple of this value (ground
	// floor, index 0, never gets one); 0 disables balconies.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony")
	int32 EveryNFloors = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony")
	FString Material = TEXT("Grammar Balconies");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony")
	FLinearColor Color = FLinearColor(0.58, 0.58, 0.55, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony")
	FString TexturePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony")
	double TextureScale = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony|Railing")
	FString RailingMaterial = TEXT("Grammar Balcony Railings");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony|Railing")
	FLinearColor RailingColor = FLinearColor(0.16, 0.16, 0.15, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony|Railing")
	int32 RailingBarCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony|Railing")
	double RailingBarWidth = 0.04;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balcony|Railing")
	double RailingBarDepth = 0.04;
};
