#pragma once

#include "CoreMinimal.h"
#include "WindowStyleConfig.generated.h"

// Port of config.py's WindowStyleConfig. Defaults match the Python dataclass exactly.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FWindowStyleConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window")
	double Width = 1.25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window")
	double Height = 1.55;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window")
	double SillHeight = 0.85;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window")
	double Spacing = 2.7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window")
	double MinMargin = 0.8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window")
	double Depth = 0.04;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window")
	FString Material = TEXT("Grammar Glass");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window")
	FLinearColor Color = FLinearColor(0.12, 0.22, 0.32, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window")
	FString TexturePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window")
	double TextureScale = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Frame")
	double FrameWidth = 0.08;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Frame")
	double FrameDepth = 0.03;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Frame")
	FString FrameMaterial = TEXT("Grammar Window Frames");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Frame")
	FLinearColor FrameColor = FLinearColor(0.86, 0.84, 0.78, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Frame")
	int32 VerticalMullions = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Frame")
	int32 HorizontalMullions = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Sill")
	double SillDepth = 0.16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Sill")
	double SillThickness = 0.06;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Sill")
	FString SillMaterial = TEXT("Grammar Window Sills");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Sill")
	FLinearColor SillColor = FLinearColor(0.78, 0.74, 0.68, 1.0);
};
