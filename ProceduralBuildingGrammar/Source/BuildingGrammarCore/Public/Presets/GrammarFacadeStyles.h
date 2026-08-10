#pragma once

#include "CoreMinimal.h"
#include "Config/FacadeStyleConfig.h"

// Port of presets.py's 31 example_facade_styles(). 25 of 31 are natively ported here (everything
// needed by every building preset in GrammarBuildingPresets.cpp, i.e. all of
// example_building_configs()); the remaining 6 residential styles (nachkriegsmoderne_residential,
// jugendstil_residential, fachwerk_townhouse, siedlung_1920s_residential,
// reihenhaus_1990s_residential, passivhaus_residential) aren't referenced by any building preset
// and are still only reachable via JSON import (see the README's preset-library section).
namespace GrammarFacadeStyles
{
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig StoneUrbanFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig QuietSideFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig BrickRowhouseFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig ModernGlassFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig SteelCurtainWallOfficeFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig ExposedSteelBraceOfficeFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig AtriumGlassTowerOfficeFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig OpenDeckParkingGarageFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig ConcreteMultistoreyParkingFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig TransitStopShelterFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig WarehouseFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig SupermarketFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig RetailShopfrontFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig IndustrialWarehouseFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig MediterraneanFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig GruenderzeitResidentialFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig BauhausResidentialFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig ContemporaryGermanResidentialFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig PlattenbauResidentialFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig GermanOfficeGridFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig KontorhausBrickOfficeFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig GothicChurchFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig CathedralStoneFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig ApartmentGabledResidentialFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig ApartmentPyramidResidentialFacade();

	// All 25 natively-ported styles, in the order above.
	BUILDINGGRAMMARCORE_API TArray<FFacadeStyleConfig> NativeFacadeStyles();
}
