#pragma once

#include "CoreMinimal.h"
#include "Config/BuildingGrammarConfig.h"

// Port of all 16 of presets.py's example_building_configs(), each referencing the corresponding
// GrammarFacadeStyles:: native facade style(s) (see that header's own comment on coverage).
namespace GrammarBuildingPresets
{
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig UrbanBlockConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig ModernMidriseConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig ModernSteelOfficeConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig LowriseMixedConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig WarehouseConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig RetailConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig SupermarketConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig IndustrialConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig ParkingGarageConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig TransitShelterConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig ChurchCathedralConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig GruenderzeitBlockConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig BauhausApartmentConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig PlattenbauSlabConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig GermanApartmentRoofsConfig();
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig GermanOfficeConfig();

	// name -> config, mirroring presets.py's example_building_configs() exactly (all 16).
	BUILDINGGRAMMARCORE_API TMap<FString, FBuildingGrammarConfig> AllBuildingPresets();
}
