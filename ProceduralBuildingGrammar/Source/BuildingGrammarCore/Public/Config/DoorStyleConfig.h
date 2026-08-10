#pragma once

#include "CoreMinimal.h"
#include "DoorStyleConfig.generated.h"

UENUM(BlueprintType)
enum class EGrammarDoorPlacement : uint8
{
	// "first_facade" and "street_facing" are historically distinct config.py values but both
	// resolve to the same behavior in grammar.py's _door_applies (only the street-facing side gets
	// a door) -- kept as one enum value here rather than reproducing the redundant pair.
	StreetFacing UMETA(DisplayName = "Street Facing"),
	EachFacade UMETA(DisplayName = "Each Facade"),
	None UMETA(DisplayName = "None")
};

// Port of config.py's DoorStyleConfig. Defaults match the Python dataclass exactly.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FDoorStyleConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	EGrammarDoorPlacement Placement = EGrammarDoorPlacement::StreetFacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	double Width = 1.25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	double Height = 2.25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	double Depth = 0.08;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FString Material = TEXT("Grammar Door");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FLinearColor Color = FLinearColor(0.16, 0.1, 0.06, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FString TexturePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	double TextureScale = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Frame")
	double FrameWidth = 0.12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Frame")
	double FrameDepth = 0.04;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Frame")
	FString FrameMaterial = TEXT("Grammar Door Frames");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Frame")
	FLinearColor FrameColor = FLinearColor(0.72, 0.68, 0.6, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Handle")
	bool bHandleEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Handle")
	double HandleRadius = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Handle")
	FString HandleMaterial = TEXT("Grammar Door Handles");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Handle")
	FLinearColor HandleColor = FLinearColor(0.82, 0.66, 0.32, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Canopy")
	bool bCanopyEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Canopy")
	double CanopyWidth = 1.8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Canopy")
	double CanopyDepth = 0.75;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Canopy")
	double CanopyThickness = 0.08;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Canopy")
	FString CanopyMaterial = TEXT("Grammar Door Canopies");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Canopy")
	FLinearColor CanopyColor = FLinearColor(0.36, 0.36, 0.34, 1.0);
};
