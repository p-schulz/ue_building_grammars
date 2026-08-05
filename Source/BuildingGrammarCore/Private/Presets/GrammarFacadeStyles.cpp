#include "Presets/GrammarFacadeStyles.h"
#include "Presets/GrammarSubStyles.h"

namespace GrammarFacadeStyles
{
	FFacadeStyleConfig StoneUrbanFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("stone_urban");
		Style.WallMaterial = TEXT("Grammar Warm Stone");
		Style.WallColor = FLinearColor(0.68, 0.62, 0.52, 1.0);
		Style.WallColorVariants = {
			FLinearColor(0.68, 0.62, 0.52, 1.0),
			FLinearColor(0.72, 0.66, 0.56, 1.0),
			FLinearColor(0.62, 0.58, 0.5, 1.0),
		};
		Style.WallColorVariantMode = EGrammarWallColorVariantMode::Building;
		Style.WallRowColors = {
			FLinearColor(0.58, 0.52, 0.44, 1.0),
			FLinearColor(0.7, 0.64, 0.54, 1.0),
			FLinearColor(0.66, 0.6, 0.5, 1.0),
		};
		Style.WallRowColorMode = EGrammarWallRowColorMode::GroundAccent;

		Style.Window.Width = 1.25;
		Style.Window.Height = 1.6;
		Style.Window.SillHeight = 0.9;
		Style.Window.Spacing = 2.8;
		Style.Window.MinMargin = 0.9;
		Style.Window.Depth = 0.05;
		Style.Window.Material = TEXT("Grammar Dark Glass");
		Style.Window.Color = FLinearColor(0.09, 0.16, 0.22, 1.0);
		Style.Window.FrameWidth = 0.09;
		Style.Window.FrameDepth = 0.04;
		Style.Window.FrameMaterial = TEXT("Grammar Bronze Window Frames");
		Style.Window.FrameColor = FLinearColor(0.36, 0.31, 0.24, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 1;
		Style.Window.SillDepth = 0.22;
		Style.Window.SillThickness = 0.07;
		Style.Window.SillMaterial = TEXT("Grammar Pale Stone Sills");
		Style.Window.SillColor = FLinearColor(0.8, 0.77, 0.7, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.18;
		Style.Ledge.Height = 0.08;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Pale Ledges");
		Style.Ledge.Color = FLinearColor(0.8, 0.77, 0.7, 1.0);

		Style.Balcony.bEnabled = true;
		Style.Balcony.Width = 2.0;
		Style.Balcony.Depth = 0.75;
		Style.Balcony.SlabHeight = 0.12;
		Style.Balcony.RailingHeight = 0.9;
		Style.Balcony.EveryNFloors = 2;
		Style.Balcony.Material = TEXT("Grammar Metal Balconies");
		Style.Balcony.Color = FLinearColor(0.42, 0.42, 0.4, 1.0);
		Style.Balcony.RailingMaterial = TEXT("Grammar Bronze Balcony Railings");
		Style.Balcony.RailingColor = FLinearColor(0.22, 0.2, 0.17, 1.0);
		Style.Balcony.RailingBarCount = 6;
		Style.Balcony.RailingBarWidth = 0.04;
		Style.Balcony.RailingBarDepth = 0.04;

		Style.Door = GrammarSubStyles::ResidentialDoorStyle();
		Style.Antenna = GrammarSubStyles::DomesticAntennaStyle(EGrammarAntennaType::Tv, 1);
		return Style;
	}

	FFacadeStyleConfig QuietSideFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("quiet_side");
		Style.WallMaterial = TEXT("Grammar Light Render");
		Style.WallColor = FLinearColor(0.76, 0.75, 0.7, 1.0);
		Style.WallColorVariants = { FLinearColor(0.76, 0.75, 0.7, 1.0), FLinearColor(0.72, 0.74, 0.7, 1.0) };
		Style.WallColorVariantMode = EGrammarWallColorVariantMode::Facade;

		Style.Window.Width = 1.05;
		Style.Window.Height = 1.35;
		Style.Window.SillHeight = 0.95;
		Style.Window.Spacing = 3.2;
		Style.Window.MinMargin = 1.0;
		Style.Window.Depth = 0.05;
		Style.Window.Material = TEXT("Grammar Dark Glass");
		Style.Window.Color = FLinearColor(0.09, 0.16, 0.22, 1.0);
		Style.Window.FrameWidth = 0.07;
		Style.Window.FrameDepth = 0.035;
		Style.Window.FrameMaterial = TEXT("Grammar Quiet White Frames");
		Style.Window.FrameColor = FLinearColor(0.86, 0.86, 0.82, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 0;
		Style.Window.SillDepth = 0.12;
		Style.Window.SillThickness = 0.05;
		Style.Window.SillMaterial = TEXT("Grammar Rendered Sills");
		Style.Window.SillColor = FLinearColor(0.78, 0.76, 0.7, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.1;
		Style.Ledge.Height = 0.06;
		Style.Ledge.EveryNFloors = 2;
		Style.Ledge.Material = TEXT("Grammar Pale Ledges");
		Style.Ledge.Color = FLinearColor(0.8, 0.77, 0.7, 1.0);

		Style.Balcony.bEnabled = false;

		Style.Door.bEnabled = false;
		Style.Door.Placement = EGrammarDoorPlacement::None;

		Style.Antenna = GrammarSubStyles::DomesticAntennaStyle(EGrammarAntennaType::Radio, 1, 1.1);
		return Style;
	}

	FFacadeStyleConfig BrickRowhouseFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("brick_rowhouse");
		Style.WallMaterial = TEXT("Grammar Red Brick");
		Style.WallColor = FLinearColor(0.55, 0.22, 0.16, 1.0);
		Style.WallColorVariants = {
			FLinearColor(0.55, 0.22, 0.16, 1.0),
			FLinearColor(0.48, 0.18, 0.13, 1.0),
			FLinearColor(0.6, 0.26, 0.18, 1.0),
		};
		Style.WallColorVariantMode = EGrammarWallColorVariantMode::Building;
		Style.WallRowColors = {
			FLinearColor(0.42, 0.15, 0.11, 1.0),
			FLinearColor(0.55, 0.22, 0.16, 1.0),
			FLinearColor(0.5, 0.2, 0.15, 1.0),
		};
		Style.WallRowColorMode = EGrammarWallRowColorMode::GroundAccent;

		Style.Window.Width = 1.0;
		Style.Window.Height = 1.45;
		Style.Window.SillHeight = 0.85;
		Style.Window.Spacing = 2.35;
		Style.Window.MinMargin = 0.75;
		Style.Window.Depth = 0.06;
		Style.Window.Material = TEXT("Grammar White Framed Glass");
		Style.Window.Color = FLinearColor(0.78, 0.86, 0.9, 1.0);
		Style.Window.FrameWidth = 0.1;
		Style.Window.FrameDepth = 0.045;
		Style.Window.FrameMaterial = TEXT("Grammar Painted Timber Frames");
		Style.Window.FrameColor = FLinearColor(0.93, 0.9, 0.82, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 1;
		Style.Window.SillDepth = 0.18;
		Style.Window.SillThickness = 0.06;
		Style.Window.SillMaterial = TEXT("Grammar Rowhouse Stone Sills");
		Style.Window.SillColor = FLinearColor(0.72, 0.69, 0.62, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.12;
		Style.Ledge.Height = 0.06;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Stone Sills");
		Style.Ledge.Color = FLinearColor(0.72, 0.69, 0.62, 1.0);

		Style.Balcony.bEnabled = false;
		Style.Balcony.Material = TEXT("Grammar Black Railings");
		Style.Balcony.Color = FLinearColor(0.05, 0.05, 0.045, 1.0);
		Style.Balcony.RailingMaterial = TEXT("Grammar Black Iron Railings");
		Style.Balcony.RailingColor = FLinearColor(0.04, 0.04, 0.035, 1.0);
		Style.Balcony.RailingBarCount = 6;
		Style.Balcony.RailingBarWidth = 0.035;
		Style.Balcony.RailingBarDepth = 0.035;

		Style.Door = GrammarSubStyles::ResidentialDoorStyle(TEXT("Grammar Rowhouse Timber Door"), FLinearColor(0.2, 0.07, 0.035, 1.0), TEXT("Grammar Rowhouse Stone Door Frames"));
		Style.Antenna = GrammarSubStyles::DomesticAntennaStyle(EGrammarAntennaType::Tv, 1, 1.2);
		return Style;
	}

	FFacadeStyleConfig ModernGlassFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("modern_glass");
		Style.WallMaterial = TEXT("Grammar Graphite Mullions");
		Style.WallColor = FLinearColor(0.16, 0.17, 0.18, 1.0);

		Style.Window.Width = 2.4;
		Style.Window.Height = 2.25;
		Style.Window.SillHeight = 0.35;
		Style.Window.Spacing = 2.75;
		Style.Window.MinMargin = 0.5;
		Style.Window.Depth = 0.03;
		Style.Window.Material = TEXT("Grammar Blue Glass");
		Style.Window.Color = FLinearColor(0.1, 0.28, 0.38, 0.88);
		Style.Window.FrameWidth = 0.055;
		Style.Window.FrameDepth = 0.035;
		Style.Window.FrameMaterial = TEXT("Grammar Graphite Window Frames");
		Style.Window.FrameColor = FLinearColor(0.08, 0.085, 0.09, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 0;
		Style.Window.SillDepth = 0.04;
		Style.Window.SillThickness = 0.03;
		Style.Window.SillMaterial = TEXT("Grammar Aluminum Drip Edges");
		Style.Window.SillColor = FLinearColor(0.5, 0.52, 0.52, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.08;
		Style.Ledge.Height = 0.05;
		Style.Ledge.EveryNFloors = 3;
		Style.Ledge.Material = TEXT("Grammar Aluminum Bands");
		Style.Ledge.Color = FLinearColor(0.58, 0.6, 0.6, 1.0);

		Style.Balcony.bEnabled = false;
		Style.Balcony.RailingMaterial = TEXT("Grammar Laminated Glass Rails");
		Style.Balcony.RailingColor = FLinearColor(0.35, 0.48, 0.52, 0.76);
		Style.Balcony.RailingBarCount = 0;
		Style.Balcony.RailingBarWidth = 0.03;
		Style.Balcony.RailingBarDepth = 0.03;

		Style.Door = GrammarSubStyles::OfficeDoorStyle();
		Style.Antenna = GrammarSubStyles::OfficeAntennaStyle(EGrammarAntennaType::Cellular, 2, 2.8);
		return Style;
	}

	FFacadeStyleConfig WarehouseFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("warehouse");
		Style.WallMaterial = TEXT("Grammar Concrete Panels");
		Style.WallColor = FLinearColor(0.5, 0.51, 0.49, 1.0);

		Style.Window.Width = 2.0;
		Style.Window.Height = 1.0;
		Style.Window.SillHeight = 1.8;
		Style.Window.Spacing = 4.0;
		Style.Window.MinMargin = 1.25;
		Style.Window.Depth = 0.04;
		Style.Window.Material = TEXT("Grammar Industrial Glass");
		Style.Window.Color = FLinearColor(0.18, 0.25, 0.28, 1.0);
		Style.Window.FrameWidth = 0.08;
		Style.Window.FrameDepth = 0.035;
		Style.Window.FrameMaterial = TEXT("Grammar Industrial Steel Frames");
		Style.Window.FrameColor = FLinearColor(0.12, 0.13, 0.13, 1.0);
		Style.Window.VerticalMullions = 2;
		Style.Window.HorizontalMullions = 1;
		Style.Window.SillDepth = 0.08;
		Style.Window.SillThickness = 0.04;
		Style.Window.SillMaterial = TEXT("Grammar Concrete Drip Sills");
		Style.Window.SillColor = FLinearColor(0.42, 0.43, 0.42, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.06;
		Style.Ledge.Height = 0.05;
		Style.Ledge.EveryNFloors = 2;
		Style.Ledge.Material = TEXT("Grammar Concrete Seams");
		Style.Ledge.Color = FLinearColor(0.42, 0.43, 0.42, 1.0);

		Style.Balcony.bEnabled = false;
		Style.Balcony.RailingMaterial = TEXT("Grammar Utility Railings");
		Style.Balcony.RailingColor = FLinearColor(0.18, 0.18, 0.17, 1.0);
		Style.Balcony.RailingBarCount = 3;
		Style.Balcony.RailingBarWidth = 0.05;
		Style.Balcony.RailingBarDepth = 0.05;

		Style.Door.bEnabled = true;
		Style.Door.Placement = EGrammarDoorPlacement::StreetFacing;
		Style.Door.Width = 2.6;
		Style.Door.Height = 3.0;
		Style.Door.Depth = 0.08;
		Style.Door.Material = TEXT("Grammar Industrial Loading Door");
		Style.Door.Color = FLinearColor(0.22, 0.24, 0.24, 1.0);
		Style.Door.FrameMaterial = TEXT("Grammar Industrial Door Frames");
		Style.Door.FrameColor = FLinearColor(0.12, 0.13, 0.13, 1.0);
		Style.Door.bCanopyEnabled = false;

		Style.Antenna = GrammarSubStyles::OfficeAntennaStyle(EGrammarAntennaType::Radio, 1, 2.0);
		return Style;
	}

	FFacadeStyleConfig IndustrialWarehouseFacade()
	{
		FFacadeStyleConfig Style = WarehouseFacade();
		Style.Name = TEXT("industrial_warehouse");
		Style.BuildingValues = { TEXT("industrial"), TEXT("warehouse"), TEXT("factory"), TEXT("manufacture") };

		FGrammarStringList BuildingFilter;
		BuildingFilter.Values = { TEXT("industrial"), TEXT("warehouse"), TEXT("factory"), TEXT("manufacture") };
		FGrammarStringList IndustrialFilter;
		IndustrialFilter.Values = { TEXT("warehouse"), TEXT("factory"), TEXT("logistics"), TEXT("manufacturing") };
		FGrammarStringList LanduseFilter;
		LanduseFilter.Values = { TEXT("industrial") };
		Style.TagFilters.Empty();
		Style.TagFilters.Add(TEXT("building"), BuildingFilter);
		Style.TagFilters.Add(TEXT("industrial"), IndustrialFilter);
		Style.TagFilters.Add(TEXT("landuse"), LanduseFilter);

		Style.bHasDefaultLevels = true;
		Style.DefaultLevels = 2;
		Style.bHasDefaultFloorHeight = true;
		Style.DefaultFloorHeight = 4.8;

		Style.bOverrideRoof = true;
		Style.RoofOverride = FRoofStyleConfig();
		Style.RoofOverride.Type = EGrammarRoofType::Flat;
		Style.RoofOverride.Height = 0.85;
		Style.RoofOverride.Overhang = 0.12;
		Style.RoofOverride.Material = TEXT("Grammar Industrial Membrane Roof");
		Style.RoofOverride.Color = FLinearColor(0.19, 0.2, 0.195, 1.0);
		Style.RoofOverride.bEdgeEnabled = true;
		Style.RoofOverride.EdgeWidth = 0.3;
		Style.RoofOverride.EdgeHeight = 0.5;
		Style.RoofOverride.SurfaceInset = 0.06;
		Style.RoofOverride.EdgeMaterial = TEXT("Grammar Industrial Roof Coping");
		Style.RoofOverride.EdgeColor = FLinearColor(0.34, 0.35, 0.33, 1.0);
		Style.RoofOverride.CornerCapSize = 0.48;

		Style.WallMaterial = TEXT("Grammar Industrial Metal Panels");
		Style.WallColor = FLinearColor(0.54, 0.56, 0.54, 1.0);
		Style.WallColorVariants = {
			FLinearColor(0.54, 0.56, 0.54, 1.0),
			FLinearColor(0.46, 0.49, 0.5, 1.0),
			FLinearColor(0.62, 0.62, 0.58, 1.0),
		};
		Style.WallColorVariantMode = EGrammarWallColorVariantMode::Building;

		Style.Ledge.Material = TEXT("Grammar Industrial Panel Seams");

		Style.Door.Width = 3.6;
		Style.Door.Height = 4.0;
		Style.Door.Material = TEXT("Grammar Sectional Loading Bay Door");

		return Style;
	}

	FFacadeStyleConfig GruenderzeitResidentialFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("gruenderzeit_residential");
		Style.WallMaterial = TEXT("Grammar Stucco Sandstone");
		Style.WallColor = FLinearColor(0.72, 0.64, 0.5, 1.0);

		Style.Window.Width = 1.05;
		Style.Window.Height = 1.85;
		Style.Window.SillHeight = 0.75;
		Style.Window.Spacing = 2.25;
		Style.Window.MinMargin = 0.65;
		Style.Window.Depth = 0.08;
		Style.Window.Material = TEXT("Grammar Tall Altbau Glass");
		Style.Window.Color = FLinearColor(0.13, 0.21, 0.25, 1.0);
		Style.Window.FrameWidth = 0.11;
		Style.Window.FrameDepth = 0.05;
		Style.Window.FrameMaterial = TEXT("Grammar Painted Altbau Frames");
		Style.Window.FrameColor = FLinearColor(0.9, 0.86, 0.76, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 1;
		Style.Window.SillDepth = 0.28;
		Style.Window.SillThickness = 0.08;
		Style.Window.SillMaterial = TEXT("Grammar Sandstone Window Sills");
		Style.Window.SillColor = FLinearColor(0.82, 0.76, 0.64, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.22;
		Style.Ledge.Height = 0.1;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Stucco Cornices");
		Style.Ledge.Color = FLinearColor(0.82, 0.76, 0.64, 1.0);

		Style.Balcony.bEnabled = true;
		Style.Balcony.Width = 1.7;
		Style.Balcony.Depth = 0.55;
		Style.Balcony.SlabHeight = 0.1;
		Style.Balcony.RailingHeight = 0.95;
		Style.Balcony.EveryNFloors = 2;
		Style.Balcony.Material = TEXT("Grammar Gruenderzeit Ironwork");
		Style.Balcony.Color = FLinearColor(0.035, 0.03, 0.025, 1.0);
		Style.Balcony.RailingMaterial = TEXT("Grammar Ornate Iron Railings");
		Style.Balcony.RailingColor = FLinearColor(0.025, 0.022, 0.02, 1.0);
		Style.Balcony.RailingBarCount = 7;
		Style.Balcony.RailingBarWidth = 0.035;
		Style.Balcony.RailingBarDepth = 0.035;

		Style.Door = GrammarSubStyles::ResidentialDoorStyle(TEXT("Grammar Altbau Timber Door"), FLinearColor(0.16, 0.075, 0.035, 1.0), TEXT("Grammar Sandstone Door Portal"), FLinearColor(0.82, 0.76, 0.64, 1.0));
		Style.Antenna = GrammarSubStyles::DomesticAntennaStyle(EGrammarAntennaType::Tv, 1, 1.25);
		return Style;
	}

	FFacadeStyleConfig PlattenbauResidentialFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("plattenbau_residential");
		Style.WallMaterial = TEXT("Grammar Prefab Concrete Panels");
		Style.WallColor = FLinearColor(0.62, 0.63, 0.6, 1.0);
		Style.WallRowColors = {
			FLinearColor(0.54, 0.55, 0.53, 1.0),
			FLinearColor(0.62, 0.63, 0.6, 1.0),
			FLinearColor(0.68, 0.68, 0.63, 1.0),
		};
		Style.WallRowColorMode = EGrammarWallRowColorMode::Cycle;

		Style.Window.Width = 1.25;
		Style.Window.Height = 1.2;
		Style.Window.SillHeight = 0.9;
		Style.Window.Spacing = 2.55;
		Style.Window.MinMargin = 0.75;
		Style.Window.Depth = 0.035;
		Style.Window.Material = TEXT("Grammar Prefab Window Glass");
		Style.Window.Color = FLinearColor(0.17, 0.25, 0.29, 1.0);
		Style.Window.FrameWidth = 0.075;
		Style.Window.FrameDepth = 0.03;
		Style.Window.FrameMaterial = TEXT("Grammar Prefab Plastic Frames");
		Style.Window.FrameColor = FLinearColor(0.86, 0.86, 0.82, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 0;
		Style.Window.SillDepth = 0.12;
		Style.Window.SillThickness = 0.04;
		Style.Window.SillMaterial = TEXT("Grammar Prefab Thin Sills");
		Style.Window.SillColor = FLinearColor(0.72, 0.72, 0.68, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.05;
		Style.Ledge.Height = 0.04;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Prefab Panel Joints");
		Style.Ledge.Color = FLinearColor(0.45, 0.46, 0.44, 1.0);

		Style.Balcony.bEnabled = true;
		Style.Balcony.Width = 2.6;
		Style.Balcony.Depth = 0.9;
		Style.Balcony.SlabHeight = 0.14;
		Style.Balcony.RailingHeight = 0.95;
		Style.Balcony.EveryNFloors = 1;
		Style.Balcony.Material = TEXT("Grammar Prefab Loggia Panels");
		Style.Balcony.Color = FLinearColor(0.78, 0.76, 0.68, 1.0);
		Style.Balcony.RailingMaterial = TEXT("Grammar Prefab Loggia Rails");
		Style.Balcony.RailingColor = FLinearColor(0.34, 0.35, 0.34, 1.0);
		Style.Balcony.RailingBarCount = 2;
		Style.Balcony.RailingBarWidth = 0.06;
		Style.Balcony.RailingBarDepth = 0.05;

		Style.Door = GrammarSubStyles::ResidentialDoorStyle(TEXT("Grammar Prefab Entrance Door"), FLinearColor(0.16, 0.18, 0.17, 1.0), TEXT("Grammar Prefab Concrete Door Frames"), FLinearColor(0.72, 0.72, 0.68, 1.0), /*bCanopyEnabled=*/true);
		Style.Antenna = GrammarSubStyles::DomesticAntennaStyle(EGrammarAntennaType::Tv, 2, 1.35);
		return Style;
	}

	FFacadeStyleConfig GothicChurchFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("gothic_church");
		Style.BuildingValues = { TEXT("church"), TEXT("chapel"), TEXT("religious") };

		FGrammarStringList BuildingFilter;
		BuildingFilter.Values = { TEXT("church"), TEXT("chapel"), TEXT("religious") };
		FGrammarStringList AmenityFilter;
		AmenityFilter.Values = { TEXT("place_of_worship") };
		FGrammarStringList ReligionFilter;
		ReligionFilter.Values = { TEXT("christian") };
		FGrammarStringList DenominationFilter;
		DenominationFilter.Values = { TEXT("catholic"), TEXT("protestant"), TEXT("evangelical"), TEXT("lutheran"), TEXT("reformed"), TEXT("orthodox") };
		Style.TagFilters.Add(TEXT("building"), BuildingFilter);
		Style.TagFilters.Add(TEXT("amenity"), AmenityFilter);
		Style.TagFilters.Add(TEXT("religion"), ReligionFilter);
		Style.TagFilters.Add(TEXT("denomination"), DenominationFilter);

		Style.bHasDefaultLevels = true;
		Style.DefaultLevels = 3;
		Style.bHasDefaultFloorHeight = true;
		Style.DefaultFloorHeight = 4.8;

		Style.bOverrideRoof = true;
		Style.RoofOverride = FRoofStyleConfig();
		Style.RoofOverride.Type = EGrammarRoofType::Gabled;
		Style.RoofOverride.Height = 3.2;
		Style.RoofOverride.Overhang = 0.28;
		Style.RoofOverride.RidgeAlignment = EGrammarRidgeAlignment::LongestAxis;
		Style.RoofOverride.Material = TEXT("Grammar Church Slate Roof");
		Style.RoofOverride.Color = FLinearColor(0.12, 0.12, 0.13, 1.0);
		Style.RoofOverride.bEdgeEnabled = false;
		Style.RoofOverride.TileRows = 10;
		Style.RoofOverride.DormerCount = 0;
		Style.RoofOverride.RoofWindowCount = 0;
		Style.RoofOverride.ChimneyCount = 0;

		Style.WallMaterial = TEXT("Grammar Church Weathered Stone");
		Style.WallColor = FLinearColor(0.56, 0.53, 0.47, 1.0);
		Style.WallColorVariants = {
			FLinearColor(0.56, 0.53, 0.47, 1.0),
			FLinearColor(0.62, 0.59, 0.52, 1.0),
			FLinearColor(0.48, 0.47, 0.43, 1.0),
		};
		Style.WallColorVariantMode = EGrammarWallColorVariantMode::Building;
		Style.WallRowColors = {
			FLinearColor(0.42, 0.4, 0.36, 1.0),
			FLinearColor(0.58, 0.55, 0.49, 1.0),
			FLinearColor(0.52, 0.5, 0.45, 1.0),
		};
		Style.WallRowColorMode = EGrammarWallRowColorMode::GroundAccent;

		Style.Window.Width = 1.1;
		Style.Window.Height = 3.15;
		Style.Window.SillHeight = 0.55;
		Style.Window.Spacing = 2.55;
		Style.Window.MinMargin = 0.95;
		Style.Window.Depth = 0.08;
		Style.Window.Material = TEXT("Grammar Church Stained Glass");
		Style.Window.Color = FLinearColor(0.12, 0.18, 0.31, 0.9);
		Style.Window.FrameWidth = 0.13;
		Style.Window.FrameDepth = 0.06;
		Style.Window.FrameMaterial = TEXT("Grammar Church Stone Tracery");
		Style.Window.FrameColor = FLinearColor(0.72, 0.69, 0.62, 1.0);
		Style.Window.VerticalMullions = 2;
		Style.Window.HorizontalMullions = 3;
		Style.Window.SillDepth = 0.24;
		Style.Window.SillThickness = 0.1;
		Style.Window.SillMaterial = TEXT("Grammar Church Stone Sills");
		Style.Window.SillColor = FLinearColor(0.7, 0.67, 0.6, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.22;
		Style.Ledge.Height = 0.11;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Church String Courses");
		Style.Ledge.Color = FLinearColor(0.68, 0.65, 0.58, 1.0);

		Style.Balcony.bEnabled = false;

		Style.Door.bEnabled = true;
		Style.Door.Placement = EGrammarDoorPlacement::StreetFacing;
		Style.Door.Width = 2.45;
		Style.Door.Height = 3.6;
		Style.Door.Depth = 0.12;
		Style.Door.Material = TEXT("Grammar Church Heavy Timber Door");
		Style.Door.Color = FLinearColor(0.12, 0.065, 0.035, 1.0);
		Style.Door.FrameWidth = 0.24;
		Style.Door.FrameDepth = 0.09;
		Style.Door.FrameMaterial = TEXT("Grammar Church Portal Stone");
		Style.Door.FrameColor = FLinearColor(0.72, 0.68, 0.58, 1.0);
		Style.Door.HandleRadius = 0.065;
		Style.Door.HandleMaterial = TEXT("Grammar Wrought Iron Door Hardware");
		Style.Door.HandleColor = FLinearColor(0.045, 0.04, 0.035, 1.0);
		Style.Door.bCanopyEnabled = false;

		Style.Antenna = GrammarSubStyles::LightningRodStyle(1, 2.6);
		return Style;
	}

	TArray<FFacadeStyleConfig> Wave1FacadeStyles()
	{
		return {
			StoneUrbanFacade(),
			QuietSideFacade(),
			BrickRowhouseFacade(),
			ModernGlassFacade(),
			WarehouseFacade(),
			IndustrialWarehouseFacade(),
			GruenderzeitResidentialFacade(),
			PlattenbauResidentialFacade(),
			GothicChurchFacade(),
		};
	}
}
