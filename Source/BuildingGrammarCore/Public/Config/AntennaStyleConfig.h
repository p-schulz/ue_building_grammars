#pragma once

#include "CoreMinimal.h"
#include "AntennaStyleConfig.generated.h"

UENUM(BlueprintType)
enum class EGrammarAntennaType : uint8
{
	Tv UMETA(DisplayName = "TV"),
	Radio UMETA(DisplayName = "Radio"),
	Satellite UMETA(DisplayName = "Satellite"),
	LightningRod UMETA(DisplayName = "Lightning Rod"),
	Cellular UMETA(DisplayName = "Cellular"),
	OfficeCluster UMETA(DisplayName = "Office Cluster"),
	Broadcast UMETA(DisplayName = "Broadcast"),
	LampPost UMETA(DisplayName = "Lamp Post")
};

// Port of config.py's AntennaStyleConfig. Defaults match the Python dataclass exactly.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FAntennaStyleConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna")
	EGrammarAntennaType Type = EGrammarAntennaType::Tv;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna")
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna")
	double MastHeight = 1.4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna")
	double MastRadius = 0.035;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna|Base")
	double BaseWidth = 0.45;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna|Base")
	double BaseDepth = 0.45;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna|Base")
	double BaseHeight = 0.18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna|Panel")
	double PanelWidth = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna|Panel")
	double PanelHeight = 0.7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna|Panel")
	double PanelDepth = 0.06;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna")
	FString Material = TEXT("Grammar Antenna Metal");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna")
	FLinearColor Color = FLinearColor(0.34, 0.34, 0.33, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna")
	FString AccentMaterial = TEXT("Grammar Antenna Panels");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Antenna")
	FLinearColor AccentColor = FLinearColor(0.78, 0.78, 0.72, 1.0);
};
