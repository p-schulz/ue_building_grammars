#include "Presets/GrammarBuildingPresets.h"
#include "Presets/GrammarFacadeStyles.h"

namespace GrammarBuildingPresets
{
	FBuildingGrammarConfig UrbanBlockConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 5;
		Config.DefaultFloorHeight = 3.1;
		Config.IrregularFloorHeights.Add(0, 4.2);
		Config.IrregularFloorHeights.Add(4, 3.6);
		Config.Styles = { GrammarFacadeStyles::StoneUrbanFacade(), GrammarFacadeStyles::QuietSideFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Gabled;
		Config.Roof.Height = 1.8;
		Config.Roof.Overhang = 0.25;
		Config.Roof.Material = TEXT("Grammar Clay Roof");
		Config.Roof.Color = FLinearColor(0.36, 0.09, 0.05, 1.0);
		Config.Roof.TileRows = 8;
		Config.Roof.DormerCount = 2;
		Config.Roof.RoofWindowCount = 2;
		Config.Roof.ChimneyCount = 1;

		return Config;
	}

	TMap<FString, FBuildingGrammarConfig> Wave1BuildingPresets()
	{
		TMap<FString, FBuildingGrammarConfig> Presets;
		Presets.Add(TEXT("urban_block"), UrbanBlockConfig());
		return Presets;
	}
}
