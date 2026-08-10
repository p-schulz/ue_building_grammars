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

	FBuildingGrammarConfig ModernMidriseConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 9;
		Config.DefaultFloorHeight = 3.4;
		Config.IrregularFloorHeights.Add(0, 4.8);
		Config.Styles = { GrammarFacadeStyles::ModernGlassFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Flat;
		Config.Roof.Height = 0.6;
		Config.Roof.Overhang = 0.1;
		Config.Roof.Material = TEXT("Grammar Dark Parapet");
		Config.Roof.Color = FLinearColor(0.12, 0.12, 0.12, 1.0);

		return Config;
	}

	FBuildingGrammarConfig ModernSteelOfficeConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 12;
		Config.DefaultFloorHeight = 3.65;
		Config.IrregularFloorHeights.Add(0, 5.4);
		Config.IrregularFloorHeights.Add(1, 4.2);
		Config.Styles = {
			GrammarFacadeStyles::SteelCurtainWallOfficeFacade(),
			GrammarFacadeStyles::ExposedSteelBraceOfficeFacade(),
			GrammarFacadeStyles::AtriumGlassTowerOfficeFacade(),
		};

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Flat;
		Config.Roof.Height = 0.75;
		Config.Roof.Overhang = 0.06;
		Config.Roof.Material = TEXT("Grammar Modern Office Service Roof");
		Config.Roof.Color = FLinearColor(0.14, 0.15, 0.15, 1.0);
		Config.Roof.bEdgeEnabled = true;
		Config.Roof.EdgeWidth = 0.36;
		Config.Roof.EdgeHeight = 0.55;
		Config.Roof.SurfaceInset = 0.1;
		Config.Roof.EdgeMaterial = TEXT("Grammar Steel Parapet Edge");
		Config.Roof.EdgeColor = FLinearColor(0.16, 0.17, 0.17, 1.0);
		Config.Roof.CornerCapSize = 0.52;

		return Config;
	}

	FBuildingGrammarConfig LowriseMixedConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 3;
		Config.DefaultFloorHeight = 3.0;
		Config.IrregularFloorHeights.Add(0, 3.8);
		Config.Styles = {
			GrammarFacadeStyles::BrickRowhouseFacade(),
			GrammarFacadeStyles::MediterraneanFacade(),
			GrammarFacadeStyles::QuietSideFacade(),
		};

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Hipped;
		Config.Roof.Height = 1.4;
		Config.Roof.Overhang = 0.35;
		Config.Roof.Material = TEXT("Grammar Warm Tile Roof");
		Config.Roof.Color = FLinearColor(0.45, 0.17, 0.08, 1.0);
		Config.Roof.TileRows = 7;
		Config.Roof.RoofWindowCount = 2;
		Config.Roof.ChimneyCount = 1;

		return Config;
	}

	FBuildingGrammarConfig WarehouseConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 2;
		Config.DefaultFloorHeight = 4.6;
		Config.Styles = { GrammarFacadeStyles::IndustrialWarehouseFacade(), GrammarFacadeStyles::WarehouseFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Flat;
		Config.Roof.Height = 0.8;
		Config.Roof.Overhang = 0.15;
		Config.Roof.Material = TEXT("Grammar Membrane Roof");
		Config.Roof.Color = FLinearColor(0.22, 0.23, 0.22, 1.0);

		return Config;
	}

	FBuildingGrammarConfig RetailConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 2;
		Config.DefaultFloorHeight = 3.8;
		Config.IrregularFloorHeights.Add(0, 4.2);
		Config.Styles = { GrammarFacadeStyles::RetailShopfrontFacade(), GrammarFacadeStyles::SupermarketFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Flat;
		Config.Roof.Height = 0.55;
		Config.Roof.Overhang = 0.12;
		Config.Roof.Material = TEXT("Grammar Retail Generic Roof");
		Config.Roof.Color = FLinearColor(0.2, 0.205, 0.2, 1.0);
		Config.Roof.bEdgeEnabled = true;
		Config.Roof.EdgeWidth = 0.34;
		Config.Roof.EdgeHeight = 0.55;
		Config.Roof.SurfaceInset = 0.07;
		Config.Roof.EdgeMaterial = TEXT("Grammar Retail Generic Parapet");
		Config.Roof.EdgeColor = FLinearColor(0.34, 0.34, 0.32, 1.0);
		Config.Roof.CornerCapSize = 0.5;

		return Config;
	}

	FBuildingGrammarConfig SupermarketConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 1;
		Config.DefaultFloorHeight = 5.2;
		Config.Styles = { GrammarFacadeStyles::SupermarketFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Flat;
		Config.Roof.Height = 0.75;
		Config.Roof.Overhang = 0.22;
		Config.Roof.Material = TEXT("Grammar Supermarket Generic Roof");
		Config.Roof.Color = FLinearColor(0.18, 0.19, 0.18, 1.0);
		Config.Roof.bEdgeEnabled = true;
		Config.Roof.EdgeWidth = 0.42;
		Config.Roof.EdgeHeight = 0.85;
		Config.Roof.SurfaceInset = 0.08;
		Config.Roof.EdgeMaterial = TEXT("Grammar Supermarket Generic Parapet");
		Config.Roof.EdgeColor = FLinearColor(0.78, 0.76, 0.68, 1.0);
		Config.Roof.CornerCapSize = 0.58;

		return Config;
	}

	FBuildingGrammarConfig IndustrialConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 2;
		Config.DefaultFloorHeight = 4.8;
		Config.Styles = { GrammarFacadeStyles::IndustrialWarehouseFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Flat;
		Config.Roof.Height = 0.85;
		Config.Roof.Overhang = 0.12;
		Config.Roof.Material = TEXT("Grammar Industrial Generic Roof");
		Config.Roof.Color = FLinearColor(0.19, 0.2, 0.195, 1.0);
		Config.Roof.bEdgeEnabled = true;
		Config.Roof.EdgeWidth = 0.3;
		Config.Roof.EdgeHeight = 0.5;
		Config.Roof.SurfaceInset = 0.06;
		Config.Roof.EdgeMaterial = TEXT("Grammar Industrial Generic Coping");
		Config.Roof.EdgeColor = FLinearColor(0.34, 0.35, 0.33, 1.0);
		Config.Roof.CornerCapSize = 0.48;

		return Config;
	}

	FBuildingGrammarConfig ParkingGarageConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 5;
		Config.DefaultFloorHeight = 3.0;
		Config.Styles = { GrammarFacadeStyles::OpenDeckParkingGarageFacade(), GrammarFacadeStyles::ConcreteMultistoreyParkingFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Flat;
		Config.Roof.Height = 0.35;
		Config.Roof.Overhang = 0.05;
		Config.Roof.Material = TEXT("Grammar Parking Generic Roof Deck");
		Config.Roof.Color = FLinearColor(0.18, 0.19, 0.18, 1.0);
		Config.Roof.bEdgeEnabled = true;
		Config.Roof.EdgeWidth = 0.28;
		Config.Roof.EdgeHeight = 0.55;
		Config.Roof.SurfaceInset = 0.04;
		Config.Roof.EdgeMaterial = TEXT("Grammar Parking Generic Parapet");
		Config.Roof.EdgeColor = FLinearColor(0.52, 0.52, 0.49, 1.0);
		Config.Roof.CornerCapSize = 0.45;

		return Config;
	}

	FBuildingGrammarConfig TransitShelterConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 1;
		Config.DefaultFloorHeight = 2.55;
		Config.Styles = { GrammarFacadeStyles::TransitStopShelterFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Flat;
		Config.Roof.Height = 0.18;
		Config.Roof.Overhang = 0.55;
		Config.Roof.Material = TEXT("Grammar Shelter Flat Roof");
		Config.Roof.Color = FLinearColor(0.08, 0.09, 0.09, 1.0);
		Config.Roof.bEdgeEnabled = true;
		Config.Roof.EdgeWidth = 0.16;
		Config.Roof.EdgeHeight = 0.16;
		Config.Roof.SurfaceInset = 0.03;
		Config.Roof.EdgeMaterial = TEXT("Grammar Shelter Roof Edge");
		Config.Roof.EdgeColor = FLinearColor(0.04, 0.045, 0.045, 1.0);
		Config.Roof.CornerCapSize = 0.22;

		return Config;
	}

	FBuildingGrammarConfig ChurchCathedralConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 4;
		Config.DefaultFloorHeight = 4.9;
		Config.IrregularFloorHeights.Add(0, 5.6);
		Config.Styles = { GrammarFacadeStyles::GothicChurchFacade(), GrammarFacadeStyles::CathedralStoneFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Gabled;
		Config.Roof.Height = 3.6;
		Config.Roof.Overhang = 0.32;
		Config.Roof.RidgeAlignment = EGrammarRidgeAlignment::LongestAxis;
		Config.Roof.Material = TEXT("Grammar Sacral Slate Roof");
		Config.Roof.Color = FLinearColor(0.095, 0.1, 0.11, 1.0);
		Config.Roof.bEdgeEnabled = false;
		Config.Roof.TileRows = 12;
		Config.Roof.DormerCount = 0;
		Config.Roof.RoofWindowCount = 0;
		Config.Roof.ChimneyCount = 0;

		return Config;
	}

	FBuildingGrammarConfig GruenderzeitBlockConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 5;
		Config.DefaultFloorHeight = 3.25;
		Config.IrregularFloorHeights.Add(0, 4.1);
		Config.Styles = { GrammarFacadeStyles::GruenderzeitResidentialFacade(), GrammarFacadeStyles::QuietSideFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Gabled;
		Config.Roof.Height = 2.0;
		Config.Roof.Overhang = 0.35;
		Config.Roof.Material = TEXT("Grammar Berlin Tile Roof");
		Config.Roof.Color = FLinearColor(0.34, 0.08, 0.045, 1.0);
		Config.Roof.TileRows = 9;
		Config.Roof.DormerCount = 3;
		Config.Roof.RoofWindowCount = 2;
		Config.Roof.ChimneyCount = 2;

		return Config;
	}

	FBuildingGrammarConfig BauhausApartmentConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 4;
		Config.DefaultFloorHeight = 3.0;
		Config.Styles = { GrammarFacadeStyles::BauhausResidentialFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Flat;
		Config.Roof.Height = 0.45;
		Config.Roof.Overhang = 0.12;
		Config.Roof.Material = TEXT("Grammar Bauhaus Flat Roof");
		Config.Roof.Color = FLinearColor(0.18, 0.18, 0.17, 1.0);

		return Config;
	}

	FBuildingGrammarConfig PlattenbauSlabConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 11;
		Config.DefaultFloorHeight = 2.8;
		Config.Styles = { GrammarFacadeStyles::PlattenbauResidentialFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Flat;
		Config.Roof.Height = 0.55;
		Config.Roof.Overhang = 0.05;
		Config.Roof.Material = TEXT("Grammar Prefab Service Roof");
		Config.Roof.Color = FLinearColor(0.24, 0.25, 0.24, 1.0);

		return Config;
	}

	FBuildingGrammarConfig GermanApartmentRoofsConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 5;
		Config.DefaultFloorHeight = 3.05;
		Config.IrregularFloorHeights.Add(0, 3.7);
		Config.Styles = { GrammarFacadeStyles::ApartmentGabledResidentialFacade(), GrammarFacadeStyles::ApartmentPyramidResidentialFacade() };

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Gabled;
		Config.Roof.Height = 1.7;
		Config.Roof.Overhang = 0.3;
		Config.Roof.RidgeAlignment = EGrammarRidgeAlignment::ClosestStreet;
		Config.Roof.Material = TEXT("Grammar Apartment Roof");
		Config.Roof.Color = FLinearColor(0.32, 0.08, 0.055, 1.0);
		Config.Roof.bEdgeEnabled = false;
		Config.Roof.TileRows = 8;
		Config.Roof.DormerCount = 2;
		Config.Roof.RoofWindowCount = 2;
		Config.Roof.ChimneyCount = 1;

		return Config;
	}

	FBuildingGrammarConfig GermanOfficeConfig()
	{
		FBuildingGrammarConfig Config;
		Config.DefaultLevels = 7;
		Config.DefaultFloorHeight = 3.45;
		Config.IrregularFloorHeights.Add(0, 4.4);
		Config.Styles = {
			GrammarFacadeStyles::GermanOfficeGridFacade(),
			GrammarFacadeStyles::SteelCurtainWallOfficeFacade(),
			GrammarFacadeStyles::ExposedSteelBraceOfficeFacade(),
			GrammarFacadeStyles::AtriumGlassTowerOfficeFacade(),
			GrammarFacadeStyles::KontorhausBrickOfficeFacade(),
		};

		Config.Roof = FRoofStyleConfig();
		Config.Roof.Type = EGrammarRoofType::Flat;
		Config.Roof.Height = 0.7;
		Config.Roof.Overhang = 0.08;
		Config.Roof.Material = TEXT("Grammar Office Roof Plant Screen");
		Config.Roof.Color = FLinearColor(0.16, 0.17, 0.16, 1.0);

		return Config;
	}

	TMap<FString, FBuildingGrammarConfig> AllBuildingPresets()
	{
		TMap<FString, FBuildingGrammarConfig> Presets;
		Presets.Add(TEXT("urban_block"), UrbanBlockConfig());
		Presets.Add(TEXT("modern_midrise"), ModernMidriseConfig());
		Presets.Add(TEXT("modern_steel_office"), ModernSteelOfficeConfig());
		Presets.Add(TEXT("lowrise_mixed"), LowriseMixedConfig());
		Presets.Add(TEXT("warehouse"), WarehouseConfig());
		Presets.Add(TEXT("retail"), RetailConfig());
		Presets.Add(TEXT("supermarket"), SupermarketConfig());
		Presets.Add(TEXT("industrial"), IndustrialConfig());
		Presets.Add(TEXT("parking_garage"), ParkingGarageConfig());
		Presets.Add(TEXT("transit_shelter"), TransitShelterConfig());
		Presets.Add(TEXT("church_cathedral"), ChurchCathedralConfig());
		Presets.Add(TEXT("gruenderzeit_block"), GruenderzeitBlockConfig());
		Presets.Add(TEXT("bauhaus_apartment"), BauhausApartmentConfig());
		Presets.Add(TEXT("plattenbau_slab"), PlattenbauSlabConfig());
		Presets.Add(TEXT("german_apartment_roofs"), GermanApartmentRoofsConfig());
		Presets.Add(TEXT("german_office"), GermanOfficeConfig());
		return Presets;
	}
}
