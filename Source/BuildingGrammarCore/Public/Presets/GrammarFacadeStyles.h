#pragma once

#include "CoreMinimal.h"
#include "Config/FacadeStyleConfig.h"

// Port of a first wave of presets.py's 31 example_facade_styles() -- the Wave 1 subset from
// docs/PLAN.md section 6, chosen to span the visual range (historic stone urban block, a quiet
// courtyard-facing side, brick rowhouse, modern glass office, plain + tag-matched industrial
// warehouse, Gründerzeit residential, socialist Plattenbau slab, and a Gothic church) and to
// validate that FFacadeStyleConfig can faithfully represent real preset content end to end. The
// remaining 23 styles are Wave 2 (not yet ported).
namespace GrammarFacadeStyles
{
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig StoneUrbanFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig QuietSideFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig BrickRowhouseFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig ModernGlassFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig WarehouseFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig IndustrialWarehouseFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig GruenderzeitResidentialFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig PlattenbauResidentialFacade();
	BUILDINGGRAMMARCORE_API FFacadeStyleConfig GothicChurchFacade();

	// All nine Wave 1 styles, in the order above.
	BUILDINGGRAMMARCORE_API TArray<FFacadeStyleConfig> Wave1FacadeStyles();
}
