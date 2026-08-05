#pragma once

#include "CoreMinimal.h"
#include "Config/DoorStyleConfig.h"
#include "Config/AntennaStyleConfig.h"

// Port of presets.py's reusable sub-style factories: three door builders (residential/office/
// shopfront) and five antenna builders (domestic/satellite/lightning-rod/office/parking-lamp),
// each parameterized the same way the Python originals are. Consumed by GrammarFacadeStyles.
namespace GrammarSubStyles
{
	BUILDINGGRAMMARCORE_API FDoorStyleConfig ResidentialDoorStyle(
		const FString& Material = TEXT("Grammar Residential Door"),
		const FLinearColor& Color = FLinearColor(0.18, 0.1, 0.055, 1.0),
		const FString& FrameMaterial = TEXT("Grammar Stone Door Frames"),
		const FLinearColor& FrameColor = FLinearColor(0.78, 0.72, 0.62, 1.0),
		bool bCanopyEnabled = false);

	BUILDINGGRAMMARCORE_API FDoorStyleConfig OfficeDoorStyle();

	BUILDINGGRAMMARCORE_API FDoorStyleConfig ShopfrontDoorStyle(
		const FString& Material = TEXT("Grammar Shopfront Entrance Glass"),
		const FLinearColor& Color = FLinearColor(0.1, 0.2, 0.24, 0.9),
		const FString& FrameMaterial = TEXT("Grammar Shopfront Metal Frames"),
		const FLinearColor& FrameColor = FLinearColor(0.08, 0.08, 0.075, 1.0));

	BUILDINGGRAMMARCORE_API FAntennaStyleConfig DomesticAntennaStyle(EGrammarAntennaType Kind = EGrammarAntennaType::Tv, int32 Count = 1, double MastHeight = 1.35);
	BUILDINGGRAMMARCORE_API FAntennaStyleConfig SatelliteAntennaStyle(int32 Count = 1, double MastHeight = 0.85);
	BUILDINGGRAMMARCORE_API FAntennaStyleConfig LightningRodStyle(int32 Count = 1, double MastHeight = 1.8);
	BUILDINGGRAMMARCORE_API FAntennaStyleConfig OfficeAntennaStyle(EGrammarAntennaType Kind = EGrammarAntennaType::Cellular, int32 Count = 2, double MastHeight = 3.2);
	BUILDINGGRAMMARCORE_API FAntennaStyleConfig ParkingLampStyle(int32 Count = 6, double MastHeight = 2.7);
}
