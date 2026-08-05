#pragma once

#include "CoreMinimal.h"
#include "Config/BuildingGrammarConfig.h"

// Port of a first wave of presets.py's 16 example_building_configs() -- see
// GrammarFacadeStyles.h's Wave 1 note. urban_block is the one building preset in this wave since
// it's the only one whose facade styles (stone_urban + quiet_side) are already ported; the
// remaining 15 (including german_office, which needs 5 more office facade styles) are Wave 2.
namespace GrammarBuildingPresets
{
	BUILDINGGRAMMARCORE_API FBuildingGrammarConfig UrbanBlockConfig();

	// name -> config, mirroring presets.py's example_building_configs() (Wave 1 subset only).
	BUILDINGGRAMMARCORE_API TMap<FString, FBuildingGrammarConfig> Wave1BuildingPresets();
}
