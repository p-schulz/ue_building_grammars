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

	FFacadeStyleConfig SteelCurtainWallOfficeFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("steel_curtain_wall_office");
		Style.WallMaterial = TEXT("Grammar Brushed Steel Curtain Wall");
		Style.WallColor = FLinearColor(0.18, 0.2, 0.21, 1.0);
		Style.WallColorVariants = {
			FLinearColor(0.18, 0.2, 0.21, 1.0),
			FLinearColor(0.16, 0.18, 0.19, 1.0),
			FLinearColor(0.22, 0.23, 0.23, 1.0),
		};
		Style.WallColorVariantMode = EGrammarWallColorVariantMode::Facade;

		Style.Window.Width = 2.85;
		Style.Window.Height = 2.75;
		Style.Window.SillHeight = 0.18;
		Style.Window.Spacing = 2.95;
		Style.Window.MinMargin = 0.35;
		Style.Window.Depth = 0.025;
		Style.Window.Material = TEXT("Grammar Full Height Blue Low-E Glass");
		Style.Window.Color = FLinearColor(0.06, 0.22, 0.3, 0.82);
		Style.Window.FrameWidth = 0.045;
		Style.Window.FrameDepth = 0.035;
		Style.Window.FrameMaterial = TEXT("Grammar Stainless Steel Curtain Mullions");
		Style.Window.FrameColor = FLinearColor(0.52, 0.54, 0.54, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 0;
		Style.Window.SillDepth = 0.035;
		Style.Window.SillThickness = 0.025;
		Style.Window.SillMaterial = TEXT("Grammar Stainless Drip Edge");
		Style.Window.SillColor = FLinearColor(0.62, 0.64, 0.64, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.07;
		Style.Ledge.Height = 0.045;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Exposed Steel Floor Edge");
		Style.Ledge.Color = FLinearColor(0.48, 0.5, 0.5, 1.0);

		Style.Balcony.bEnabled = false;

		Style.Door = GrammarSubStyles::OfficeDoorStyle();
		Style.Antenna = GrammarSubStyles::OfficeAntennaStyle(EGrammarAntennaType::OfficeCluster, 2, 3.6);
		return Style;
	}

	FFacadeStyleConfig ExposedSteelBraceOfficeFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("exposed_steel_brace_office");
		Style.WallMaterial = TEXT("Grammar Dark Exposed Steel Frame");
		Style.WallColor = FLinearColor(0.12, 0.13, 0.135, 1.0);
		Style.WallRowColors = {
			FLinearColor(0.08, 0.085, 0.09, 1.0),
			FLinearColor(0.13, 0.14, 0.145, 1.0),
		};
		Style.WallRowColorMode = EGrammarWallRowColorMode::GroundAccent;

		Style.Window.Width = 3.2;
		Style.Window.Height = 2.55;
		Style.Window.SillHeight = 0.28;
		Style.Window.Spacing = 3.35;
		Style.Window.MinMargin = 0.45;
		Style.Window.Depth = 0.03;
		Style.Window.Material = TEXT("Grammar Clear Structural Glass");
		Style.Window.Color = FLinearColor(0.08, 0.26, 0.32, 0.78);
		Style.Window.FrameWidth = 0.075;
		Style.Window.FrameDepth = 0.055;
		Style.Window.FrameMaterial = TEXT("Grammar Black Steel Mega Frame");
		Style.Window.FrameColor = FLinearColor(0.035, 0.038, 0.04, 1.0);
		Style.Window.VerticalMullions = 2;
		Style.Window.HorizontalMullions = 1;
		Style.Window.SillDepth = 0.045;
		Style.Window.SillThickness = 0.035;
		Style.Window.SillMaterial = TEXT("Grammar Black Steel Sills");
		Style.Window.SillColor = FLinearColor(0.05, 0.052, 0.052, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.12;
		Style.Ledge.Height = 0.08;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Black Steel Belt Truss");
		Style.Ledge.Color = FLinearColor(0.035, 0.038, 0.04, 1.0);

		Style.Balcony.bEnabled = false;

		Style.Door = GrammarSubStyles::OfficeDoorStyle();
		Style.Antenna = GrammarSubStyles::OfficeAntennaStyle(EGrammarAntennaType::Cellular, 3, 3.8);
		return Style;
	}

	FFacadeStyleConfig AtriumGlassTowerOfficeFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("atrium_glass_tower_office");
		Style.WallMaterial = TEXT("Grammar Pale Steel Atrium Grid");
		Style.WallColor = FLinearColor(0.7, 0.72, 0.7, 1.0);

		Style.Window.Width = 3.6;
		Style.Window.Height = 3.05;
		Style.Window.SillHeight = 0.05;
		Style.Window.Spacing = 3.75;
		Style.Window.MinMargin = 0.3;
		Style.Window.Depth = 0.02;
		Style.Window.Material = TEXT("Grammar Tall Atrium Reflective Glass");
		Style.Window.Color = FLinearColor(0.12, 0.34, 0.42, 0.72);
		Style.Window.FrameWidth = 0.04;
		Style.Window.FrameDepth = 0.035;
		Style.Window.FrameMaterial = TEXT("Grammar Satin Steel Atrium Mullions");
		Style.Window.FrameColor = FLinearColor(0.68, 0.7, 0.68, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 1;
		Style.Window.SillDepth = 0.025;
		Style.Window.SillThickness = 0.02;
		Style.Window.SillMaterial = TEXT("Grammar Satin Steel Drip Edge");
		Style.Window.SillColor = FLinearColor(0.72, 0.74, 0.72, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.05;
		Style.Ledge.Height = 0.04;
		Style.Ledge.EveryNFloors = 2;
		Style.Ledge.Material = TEXT("Grammar Pale Steel Atrium Floor Line");
		Style.Ledge.Color = FLinearColor(0.74, 0.75, 0.72, 1.0);

		Style.Balcony.bEnabled = false;

		Style.Door = GrammarSubStyles::ShopfrontDoorStyle(TEXT("Grammar Double Height Office Lobby Glass"), FLinearColor(0.08, 0.24, 0.3, 0.82), TEXT("Grammar Satin Steel Lobby Frames"), FLinearColor(0.66, 0.68, 0.66, 1.0));
		Style.Antenna = GrammarSubStyles::OfficeAntennaStyle(EGrammarAntennaType::OfficeCluster, 3, 4.0);
		return Style;
	}

	FFacadeStyleConfig OpenDeckParkingGarageFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("open_deck_parking_garage");
		Style.BuildingValues = { TEXT("parking"), TEXT("parking_garage"), TEXT("car_park") };

		FGrammarStringList AmenityFilter;
		AmenityFilter.Values = { TEXT("parking") };
		FGrammarStringList ParkingFilter;
		ParkingFilter.Values = { TEXT("multi-storey"), TEXT("multistorey"), TEXT("garage") };
		Style.TagFilters.Add(TEXT("amenity"), AmenityFilter);
		Style.TagFilters.Add(TEXT("parking"), ParkingFilter);

		Style.bHasDefaultLevels = true;
		Style.DefaultLevels = 5;
		Style.bHasDefaultFloorHeight = true;
		Style.DefaultFloorHeight = 3.0;

		Style.bOverrideRoof = true;
		Style.RoofOverride = FRoofStyleConfig();
		Style.RoofOverride.Type = EGrammarRoofType::Flat;
		Style.RoofOverride.Height = 0.35;
		Style.RoofOverride.Overhang = 0.05;
		Style.RoofOverride.Material = TEXT("Grammar Parking Top Deck Asphalt");
		Style.RoofOverride.Color = FLinearColor(0.18, 0.19, 0.18, 1.0);
		Style.RoofOverride.bEdgeEnabled = true;
		Style.RoofOverride.EdgeWidth = 0.28;
		Style.RoofOverride.EdgeHeight = 0.55;
		Style.RoofOverride.SurfaceInset = 0.04;
		Style.RoofOverride.EdgeMaterial = TEXT("Grammar Parking Concrete Parapet");
		Style.RoofOverride.EdgeColor = FLinearColor(0.55, 0.55, 0.52, 1.0);
		Style.RoofOverride.CornerCapSize = 0.45;

		Style.WallMaterial = TEXT("Grammar Open Parking Void");
		Style.WallColor = FLinearColor(0.18, 0.18, 0.17, 0.22);
		Style.WallRowColors = {
			FLinearColor(0.48, 0.48, 0.45, 1.0),
			FLinearColor(0.18, 0.18, 0.17, 0.18),
		};
		Style.WallRowColorMode = EGrammarWallRowColorMode::GroundAccent;

		Style.Window.Width = 3.4;
		Style.Window.Height = 1.85;
		Style.Window.SillHeight = 0.55;
		Style.Window.Spacing = 3.55;
		Style.Window.MinMargin = 0.35;
		Style.Window.Depth = 0.025;
		Style.Window.Material = TEXT("Grammar Open Parking Bays");
		Style.Window.Color = FLinearColor(0.08, 0.09, 0.09, 0.18);
		Style.Window.FrameWidth = 0.07;
		Style.Window.FrameDepth = 0.05;
		Style.Window.FrameMaterial = TEXT("Grammar Galvanized Parking Steel");
		Style.Window.FrameColor = FLinearColor(0.42, 0.44, 0.42, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 0;
		Style.Window.SillDepth = 0.1;
		Style.Window.SillThickness = 0.06;
		Style.Window.SillMaterial = TEXT("Grammar Parking Edge Beam");
		Style.Window.SillColor = FLinearColor(0.54, 0.54, 0.5, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.2;
		Style.Ledge.Height = 0.13;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Parking Floor Slab Edges");
		Style.Ledge.Color = FLinearColor(0.5, 0.5, 0.47, 1.0);

		Style.Balcony.bEnabled = true;
		Style.Balcony.Width = 3.2;
		Style.Balcony.Depth = 0.16;
		Style.Balcony.SlabHeight = 0.08;
		Style.Balcony.RailingHeight = 0.72;
		Style.Balcony.EveryNFloors = 1;
		Style.Balcony.Material = TEXT("Grammar Parking Guard Rail");
		Style.Balcony.Color = FLinearColor(0.42, 0.43, 0.4, 1.0);
		Style.Balcony.RailingMaterial = TEXT("Grammar Parking Guard Rail Steel");
		Style.Balcony.RailingColor = FLinearColor(0.24, 0.25, 0.24, 1.0);
		Style.Balcony.RailingBarCount = 4;
		Style.Balcony.RailingBarWidth = 0.045;
		Style.Balcony.RailingBarDepth = 0.04;

		Style.Door.bEnabled = true;
		Style.Door.Placement = EGrammarDoorPlacement::StreetFacing;
		Style.Door.Width = 2.7;
		Style.Door.Height = 2.6;
		Style.Door.Depth = 0.06;
		Style.Door.Material = TEXT("Grammar Parking Pedestrian Core Door");
		Style.Door.Color = FLinearColor(0.08, 0.1, 0.1, 1.0);
		Style.Door.FrameWidth = 0.08;
		Style.Door.FrameDepth = 0.045;
		Style.Door.FrameMaterial = TEXT("Grammar Parking Door Steel Frames");
		Style.Door.FrameColor = FLinearColor(0.18, 0.19, 0.18, 1.0);
		Style.Door.HandleRadius = 0.04;
		Style.Door.HandleMaterial = TEXT("Grammar Stainless Door Handles");
		Style.Door.HandleColor = FLinearColor(0.72, 0.72, 0.66, 1.0);

		Style.Antenna = GrammarSubStyles::ParkingLampStyle(6, 2.6);
		return Style;
	}

	FFacadeStyleConfig ConcreteMultistoreyParkingFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("concrete_multistorey_parking");
		Style.BuildingValues = { TEXT("parking"), TEXT("parking_garage"), TEXT("car_park") };

		FGrammarStringList AmenityFilter;
		AmenityFilter.Values = { TEXT("parking") };
		FGrammarStringList ParkingFilter;
		ParkingFilter.Values = { TEXT("multi-storey"), TEXT("multistorey"), TEXT("garage") };
		Style.TagFilters.Add(TEXT("amenity"), AmenityFilter);
		Style.TagFilters.Add(TEXT("parking"), ParkingFilter);

		Style.bHasDefaultLevels = true;
		Style.DefaultLevels = 6;
		Style.bHasDefaultFloorHeight = true;
		Style.DefaultFloorHeight = 2.9;

		Style.bOverrideRoof = true;
		Style.RoofOverride = FRoofStyleConfig();
		Style.RoofOverride.Type = EGrammarRoofType::Flat;
		Style.RoofOverride.Height = 0.4;
		Style.RoofOverride.Overhang = 0.03;
		Style.RoofOverride.Material = TEXT("Grammar Parking Roof Deck");
		Style.RoofOverride.Color = FLinearColor(0.2, 0.205, 0.2, 1.0);
		Style.RoofOverride.bEdgeEnabled = true;
		Style.RoofOverride.EdgeWidth = 0.32;
		Style.RoofOverride.EdgeHeight = 0.62;
		Style.RoofOverride.SurfaceInset = 0.05;
		Style.RoofOverride.EdgeMaterial = TEXT("Grammar Heavy Parking Parapet");
		Style.RoofOverride.EdgeColor = FLinearColor(0.46, 0.46, 0.43, 1.0);
		Style.RoofOverride.CornerCapSize = 0.5;

		Style.WallMaterial = TEXT("Grammar Ribbed Parking Concrete");
		Style.WallColor = FLinearColor(0.48, 0.48, 0.45, 1.0);

		Style.Window.Width = 4.1;
		Style.Window.Height = 1.35;
		Style.Window.SillHeight = 0.78;
		Style.Window.Spacing = 4.35;
		Style.Window.MinMargin = 0.5;
		Style.Window.Depth = 0.04;
		Style.Window.Material = TEXT("Grammar Dark Parking Openings");
		Style.Window.Color = FLinearColor(0.045, 0.05, 0.05, 0.35);
		Style.Window.FrameWidth = 0.1;
		Style.Window.FrameDepth = 0.055;
		Style.Window.FrameMaterial = TEXT("Grammar Precast Parking Fins");
		Style.Window.FrameColor = FLinearColor(0.62, 0.62, 0.58, 1.0);
		Style.Window.VerticalMullions = 2;
		Style.Window.HorizontalMullions = 0;
		Style.Window.SillDepth = 0.18;
		Style.Window.SillThickness = 0.08;
		Style.Window.SillMaterial = TEXT("Grammar Concrete Vehicle Barrier");
		Style.Window.SillColor = FLinearColor(0.56, 0.56, 0.52, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.18;
		Style.Ledge.Height = 0.12;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Precast Floor Bands");
		Style.Ledge.Color = FLinearColor(0.58, 0.58, 0.54, 1.0);

		Style.Balcony.bEnabled = true;
		Style.Balcony.Width = 3.7;
		Style.Balcony.Depth = 0.12;
		Style.Balcony.SlabHeight = 0.06;
		Style.Balcony.RailingHeight = 0.65;
		Style.Balcony.EveryNFloors = 1;
		Style.Balcony.Material = TEXT("Grammar Concrete Parking Rail");
		Style.Balcony.Color = FLinearColor(0.5, 0.5, 0.47, 1.0);
		Style.Balcony.RailingMaterial = TEXT("Grammar Parking Cable Rails");
		Style.Balcony.RailingColor = FLinearColor(0.26, 0.27, 0.26, 1.0);
		Style.Balcony.RailingBarCount = 5;
		Style.Balcony.RailingBarWidth = 0.035;
		Style.Balcony.RailingBarDepth = 0.035;

		Style.Door.bEnabled = true;
		Style.Door.Placement = EGrammarDoorPlacement::StreetFacing;
		Style.Door.Width = 3.2;
		Style.Door.Height = 2.8;
		Style.Door.Depth = 0.06;
		Style.Door.Material = TEXT("Grammar Parking Stair Core Glazing");
		Style.Door.Color = FLinearColor(0.1, 0.18, 0.2, 0.85);
		Style.Door.FrameWidth = 0.09;
		Style.Door.FrameDepth = 0.045;
		Style.Door.FrameMaterial = TEXT("Grammar Parking Stair Core Steel");
		Style.Door.FrameColor = FLinearColor(0.12, 0.13, 0.13, 1.0);
		Style.Door.HandleRadius = 0.04;

		Style.Antenna = GrammarSubStyles::ParkingLampStyle(8, 2.9);
		return Style;
	}

	FFacadeStyleConfig TransitStopShelterFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("transit_stop_shelter");
		Style.BuildingValues = { TEXT("shelter") };

		FGrammarStringList AmenityFilter;
		AmenityFilter.Values = { TEXT("shelter") };
		FGrammarStringList PublicTransportFilter;
		PublicTransportFilter.Values = { TEXT("platform"), TEXT("stop_position") };
		FGrammarStringList ShelterTypeFilter;
		ShelterTypeFilter.Values = { TEXT("public_transport") };
		Style.TagFilters.Add(TEXT("amenity"), AmenityFilter);
		Style.TagFilters.Add(TEXT("public_transport"), PublicTransportFilter);
		Style.TagFilters.Add(TEXT("shelter_type"), ShelterTypeFilter);

		Style.bHasDefaultLevels = true;
		Style.DefaultLevels = 1;
		Style.bHasDefaultFloorHeight = true;
		Style.DefaultFloorHeight = 2.55;

		Style.bOverrideRoof = true;
		Style.RoofOverride = FRoofStyleConfig();
		Style.RoofOverride.Type = EGrammarRoofType::Flat;
		Style.RoofOverride.Height = 0.18;
		Style.RoofOverride.Overhang = 0.55;
		Style.RoofOverride.Material = TEXT("Grammar Shelter Flat Roof");
		Style.RoofOverride.Color = FLinearColor(0.08, 0.09, 0.09, 1.0);
		Style.RoofOverride.bEdgeEnabled = true;
		Style.RoofOverride.EdgeWidth = 0.16;
		Style.RoofOverride.EdgeHeight = 0.16;
		Style.RoofOverride.SurfaceInset = 0.03;
		Style.RoofOverride.EdgeMaterial = TEXT("Grammar Shelter Roof Edge");
		Style.RoofOverride.EdgeColor = FLinearColor(0.04, 0.045, 0.045, 1.0);
		Style.RoofOverride.CornerCapSize = 0.22;

		Style.WallMaterial = TEXT("Grammar Shelter Open Side");
		Style.WallColor = FLinearColor(0.04, 0.045, 0.045, 0.12);

		Style.Window.Width = 1.85;
		Style.Window.Height = 1.75;
		Style.Window.SillHeight = 0.25;
		Style.Window.Spacing = 2.05;
		Style.Window.MinMargin = 0.25;
		Style.Window.Depth = 0.025;
		Style.Window.Material = TEXT("Grammar Shelter Safety Glass");
		Style.Window.Color = FLinearColor(0.16, 0.34, 0.42, 0.46);
		Style.Window.FrameWidth = 0.055;
		Style.Window.FrameDepth = 0.04;
		Style.Window.FrameMaterial = TEXT("Grammar Shelter Steel Frames");
		Style.Window.FrameColor = FLinearColor(0.06, 0.065, 0.065, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 0;
		Style.Window.SillDepth = 0.08;
		Style.Window.SillThickness = 0.04;
		Style.Window.SillMaterial = TEXT("Grammar Shelter Lower Rail");
		Style.Window.SillColor = FLinearColor(0.08, 0.085, 0.08, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.08;
		Style.Ledge.Height = 0.05;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Shelter Bench Rail");
		Style.Ledge.Color = FLinearColor(0.18, 0.18, 0.16, 1.0);

		Style.Balcony.bEnabled = false;

		Style.Door.bEnabled = false;
		Style.Door.Placement = EGrammarDoorPlacement::None;

		Style.Antenna.bEnabled = false;
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

	FFacadeStyleConfig SupermarketFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("supermarket");
		Style.BuildingValues = { TEXT("supermarket") };

		FGrammarStringList ShopFilter;
		ShopFilter.Values = { TEXT("supermarket"), TEXT("grocery"), TEXT("convenience"), TEXT("discount") };
		FGrammarStringList AmenityFilter;
		AmenityFilter.Values = { TEXT("marketplace") };
		FGrammarStringList BuildingFilter;
		BuildingFilter.Values = { TEXT("supermarket") };
		Style.TagFilters.Add(TEXT("shop"), ShopFilter);
		Style.TagFilters.Add(TEXT("amenity"), AmenityFilter);
		Style.TagFilters.Add(TEXT("building"), BuildingFilter);

		Style.bHasDefaultLevels = true;
		Style.DefaultLevels = 1;
		Style.bHasDefaultFloorHeight = true;
		Style.DefaultFloorHeight = 5.2;

		Style.bOverrideRoof = true;
		Style.RoofOverride = FRoofStyleConfig();
		Style.RoofOverride.Type = EGrammarRoofType::Flat;
		Style.RoofOverride.Height = 0.75;
		Style.RoofOverride.Overhang = 0.22;
		Style.RoofOverride.Material = TEXT("Grammar Supermarket Membrane Roof");
		Style.RoofOverride.Color = FLinearColor(0.18, 0.19, 0.18, 1.0);
		Style.RoofOverride.bEdgeEnabled = true;
		Style.RoofOverride.EdgeWidth = 0.42;
		Style.RoofOverride.EdgeHeight = 0.85;
		Style.RoofOverride.SurfaceInset = 0.08;
		Style.RoofOverride.EdgeMaterial = TEXT("Grammar Supermarket Sign Parapet");
		Style.RoofOverride.EdgeColor = FLinearColor(0.78, 0.76, 0.68, 1.0);
		Style.RoofOverride.CornerCapSize = 0.58;

		Style.WallMaterial = TEXT("Grammar Supermarket Light Cladding");
		Style.WallColor = FLinearColor(0.82, 0.81, 0.76, 1.0);
		Style.WallColorVariants = {
			FLinearColor(0.82, 0.81, 0.76, 1.0),
			FLinearColor(0.74, 0.78, 0.76, 1.0),
			FLinearColor(0.86, 0.84, 0.78, 1.0),
		};
		Style.WallColorVariantMode = EGrammarWallColorVariantMode::Building;
		Style.WallRowColors = {
			FLinearColor(0.24, 0.26, 0.25, 1.0),
			FLinearColor(0.82, 0.81, 0.76, 1.0),
		};
		Style.WallRowColorMode = EGrammarWallRowColorMode::GroundAccent;

		Style.Window.Width = 3.4;
		Style.Window.Height = 2.45;
		Style.Window.SillHeight = 0.35;
		Style.Window.Spacing = 4.0;
		Style.Window.MinMargin = 0.7;
		Style.Window.Depth = 0.035;
		Style.Window.Material = TEXT("Grammar Supermarket Storefront Glass");
		Style.Window.Color = FLinearColor(0.12, 0.28, 0.32, 0.72);
		Style.Window.FrameWidth = 0.075;
		Style.Window.FrameDepth = 0.045;
		Style.Window.FrameMaterial = TEXT("Grammar Supermarket Aluminum Frames");
		Style.Window.FrameColor = FLinearColor(0.12, 0.13, 0.125, 1.0);
		Style.Window.VerticalMullions = 2;
		Style.Window.HorizontalMullions = 1;
		Style.Window.SillDepth = 0.12;
		Style.Window.SillThickness = 0.05;
		Style.Window.SillMaterial = TEXT("Grammar Supermarket Base Sill");
		Style.Window.SillColor = FLinearColor(0.32, 0.33, 0.31, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.12;
		Style.Ledge.Height = 0.08;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Supermarket Fascia Band");
		Style.Ledge.Color = FLinearColor(0.24, 0.25, 0.24, 1.0);

		Style.Balcony.bEnabled = false;

		Style.Door = GrammarSubStyles::ShopfrontDoorStyle(TEXT("Grammar Automatic Supermarket Doors"), FLinearColor(0.1, 0.24, 0.28, 0.78), TEXT("Grammar Supermarket Door Frames"), FLinearColor(0.1, 0.105, 0.1, 1.0));
		Style.Antenna.bEnabled = false;
		return Style;
	}

	FFacadeStyleConfig RetailShopfrontFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("retail_shopfront");
		Style.BuildingValues = { TEXT("retail"), TEXT("commercial") };

		FGrammarStringList ShopFilter;
		ShopFilter.Values = { TEXT("*"), TEXT("mall"), TEXT("department_store"), TEXT("clothes"), TEXT("bakery"), TEXT("chemist"), TEXT("hardware") };
		FGrammarStringList BuildingFilter;
		BuildingFilter.Values = { TEXT("retail"), TEXT("commercial") };
		FGrammarStringList LanduseFilter;
		LanduseFilter.Values = { TEXT("retail") };
		Style.TagFilters.Add(TEXT("shop"), ShopFilter);
		Style.TagFilters.Add(TEXT("building"), BuildingFilter);
		Style.TagFilters.Add(TEXT("landuse"), LanduseFilter);

		Style.bHasDefaultLevels = true;
		Style.DefaultLevels = 2;
		Style.bHasDefaultFloorHeight = true;
		Style.DefaultFloorHeight = 3.8;

		Style.bOverrideRoof = true;
		Style.RoofOverride = FRoofStyleConfig();
		Style.RoofOverride.Type = EGrammarRoofType::Flat;
		Style.RoofOverride.Height = 0.55;
		Style.RoofOverride.Overhang = 0.12;
		Style.RoofOverride.Material = TEXT("Grammar Retail Service Roof");
		Style.RoofOverride.Color = FLinearColor(0.2, 0.205, 0.2, 1.0);
		Style.RoofOverride.bEdgeEnabled = true;
		Style.RoofOverride.EdgeWidth = 0.34;
		Style.RoofOverride.EdgeHeight = 0.55;
		Style.RoofOverride.SurfaceInset = 0.07;
		Style.RoofOverride.EdgeMaterial = TEXT("Grammar Retail Parapet");
		Style.RoofOverride.EdgeColor = FLinearColor(0.34, 0.34, 0.32, 1.0);
		Style.RoofOverride.CornerCapSize = 0.5;

		Style.WallMaterial = TEXT("Grammar Retail Mixed Cladding");
		Style.WallColor = FLinearColor(0.68, 0.67, 0.62, 1.0);
		Style.WallColorVariants = {
			FLinearColor(0.68, 0.67, 0.62, 1.0),
			FLinearColor(0.56, 0.6, 0.58, 1.0),
			FLinearColor(0.74, 0.7, 0.62, 1.0),
		};
		Style.WallColorVariantMode = EGrammarWallColorVariantMode::Facade;
		Style.WallRowColors = {
			FLinearColor(0.18, 0.18, 0.17, 1.0),
			FLinearColor(0.7, 0.69, 0.64, 1.0),
		};
		Style.WallRowColorMode = EGrammarWallRowColorMode::GroundAccent;

		Style.Window.Width = 2.5;
		Style.Window.Height = 2.25;
		Style.Window.SillHeight = 0.25;
		Style.Window.Spacing = 3.1;
		Style.Window.MinMargin = 0.55;
		Style.Window.Depth = 0.04;
		Style.Window.Material = TEXT("Grammar Retail Display Glass");
		Style.Window.Color = FLinearColor(0.12, 0.24, 0.29, 0.76);
		Style.Window.FrameWidth = 0.075;
		Style.Window.FrameDepth = 0.045;
		Style.Window.FrameMaterial = TEXT("Grammar Retail Mullions");
		Style.Window.FrameColor = FLinearColor(0.11, 0.11, 0.105, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 1;
		Style.Window.SillDepth = 0.1;
		Style.Window.SillThickness = 0.045;
		Style.Window.SillMaterial = TEXT("Grammar Retail Stone Base");
		Style.Window.SillColor = FLinearColor(0.46, 0.45, 0.42, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.14;
		Style.Ledge.Height = 0.07;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Retail Sign Band");
		Style.Ledge.Color = FLinearColor(0.28, 0.28, 0.26, 1.0);

		Style.Balcony.bEnabled = false;

		Style.Door = GrammarSubStyles::ShopfrontDoorStyle();
		Style.Antenna.bEnabled = false;
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

	FFacadeStyleConfig MediterraneanFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("mediterranean");
		Style.WallMaterial = TEXT("Grammar Lime Plaster");
		Style.WallColor = FLinearColor(0.84, 0.78, 0.62, 1.0);

		Style.Window.Width = 0.9;
		Style.Window.Height = 1.35;
		Style.Window.SillHeight = 0.85;
		Style.Window.Spacing = 2.4;
		Style.Window.MinMargin = 0.85;
		Style.Window.Depth = 0.08;
		Style.Window.Material = TEXT("Grammar Green Shutters Glass");
		Style.Window.Color = FLinearColor(0.16, 0.28, 0.2, 1.0);
		Style.Window.FrameWidth = 0.085;
		Style.Window.FrameDepth = 0.04;
		Style.Window.FrameMaterial = TEXT("Grammar Green Painted Frames");
		Style.Window.FrameColor = FLinearColor(0.12, 0.24, 0.16, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 0;
		Style.Window.SillDepth = 0.2;
		Style.Window.SillThickness = 0.07;
		Style.Window.SillMaterial = TEXT("Grammar Stucco Window Sills");
		Style.Window.SillColor = FLinearColor(0.92, 0.86, 0.72, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.16;
		Style.Ledge.Height = 0.08;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Stucco Sills");
		Style.Ledge.Color = FLinearColor(0.92, 0.86, 0.72, 1.0);

		Style.Balcony.bEnabled = true;
		Style.Balcony.Width = 1.55;
		Style.Balcony.Depth = 0.55;
		Style.Balcony.SlabHeight = 0.1;
		Style.Balcony.RailingHeight = 0.8;
		Style.Balcony.EveryNFloors = 2;
		Style.Balcony.Material = TEXT("Grammar Wrought Iron");
		Style.Balcony.Color = FLinearColor(0.04, 0.035, 0.03, 1.0);
		Style.Balcony.RailingMaterial = TEXT("Grammar Wrought Iron Railings");
		Style.Balcony.RailingColor = FLinearColor(0.03, 0.028, 0.024, 1.0);
		Style.Balcony.RailingBarCount = 7;
		Style.Balcony.RailingBarWidth = 0.035;
		Style.Balcony.RailingBarDepth = 0.035;

		Style.Door = GrammarSubStyles::ResidentialDoorStyle(TEXT("Grammar Mediterranean Timber Door"), FLinearColor(0.18, 0.12, 0.055, 1.0), TEXT("Grammar Stucco Door Frames"), FLinearColor(0.92, 0.86, 0.72, 1.0));
		Style.Antenna = GrammarSubStyles::DomesticAntennaStyle(EGrammarAntennaType::Tv, 1, 1.0);
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

	FFacadeStyleConfig BauhausResidentialFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("bauhaus_residential");
		Style.WallMaterial = TEXT("Grammar Bauhaus White Render");
		Style.WallColor = FLinearColor(0.9, 0.89, 0.84, 1.0);

		Style.Window.Width = 1.95;
		Style.Window.Height = 1.15;
		Style.Window.SillHeight = 1.05;
		Style.Window.Spacing = 2.35;
		Style.Window.MinMargin = 0.55;
		Style.Window.Depth = 0.035;
		Style.Window.Material = TEXT("Grammar Bauhaus Dark Frames");
		Style.Window.Color = FLinearColor(0.08, 0.1, 0.11, 1.0);
		Style.Window.FrameWidth = 0.06;
		Style.Window.FrameDepth = 0.035;
		Style.Window.FrameMaterial = TEXT("Grammar Bauhaus Steel Frames");
		Style.Window.FrameColor = FLinearColor(0.06, 0.065, 0.065, 1.0);
		Style.Window.VerticalMullions = 2;
		Style.Window.HorizontalMullions = 0;
		Style.Window.SillDepth = 0.1;
		Style.Window.SillThickness = 0.04;
		Style.Window.SillMaterial = TEXT("Grammar Bauhaus Thin Sills");
		Style.Window.SillColor = FLinearColor(0.72, 0.72, 0.68, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.08;
		Style.Ledge.Height = 0.05;
		Style.Ledge.EveryNFloors = 2;
		Style.Ledge.Material = TEXT("Grammar Bauhaus Concrete Bands");
		Style.Ledge.Color = FLinearColor(0.72, 0.72, 0.68, 1.0);

		Style.Balcony.bEnabled = true;
		Style.Balcony.Width = 2.2;
		Style.Balcony.Depth = 0.7;
		Style.Balcony.SlabHeight = 0.12;
		Style.Balcony.RailingHeight = 0.85;
		Style.Balcony.EveryNFloors = 2;
		Style.Balcony.Material = TEXT("Grammar Bauhaus Railings");
		Style.Balcony.Color = FLinearColor(0.12, 0.12, 0.11, 1.0);
		Style.Balcony.RailingMaterial = TEXT("Grammar Bauhaus Tubular Railings");
		Style.Balcony.RailingColor = FLinearColor(0.08, 0.08, 0.075, 1.0);
		Style.Balcony.RailingBarCount = 4;
		Style.Balcony.RailingBarWidth = 0.045;
		Style.Balcony.RailingBarDepth = 0.045;

		Style.Door = GrammarSubStyles::ResidentialDoorStyle(TEXT("Grammar Bauhaus Entrance Door"), FLinearColor(0.07, 0.075, 0.075, 1.0), TEXT("Grammar Bauhaus Door Frames"), FLinearColor(0.06, 0.065, 0.065, 1.0), /*bCanopyEnabled=*/true);
		Style.Antenna = GrammarSubStyles::DomesticAntennaStyle(EGrammarAntennaType::Radio, 1, 1.05);
		return Style;
	}

	FFacadeStyleConfig ContemporaryGermanResidentialFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("contemporary_german_residential");
		Style.WallMaterial = TEXT("Grammar Contemporary Mineral Render");
		Style.WallColor = FLinearColor(0.82, 0.82, 0.78, 1.0);

		Style.Window.Width = 1.65;
		Style.Window.Height = 1.9;
		Style.Window.SillHeight = 0.45;
		Style.Window.Spacing = 2.85;
		Style.Window.MinMargin = 0.75;
		Style.Window.Depth = 0.035;
		Style.Window.Material = TEXT("Grammar Contemporary Triple Glazing");
		Style.Window.Color = FLinearColor(0.12, 0.22, 0.26, 1.0);
		Style.Window.FrameWidth = 0.065;
		Style.Window.FrameDepth = 0.035;
		Style.Window.FrameMaterial = TEXT("Grammar Contemporary Anthracite Frames");
		Style.Window.FrameColor = FLinearColor(0.08, 0.085, 0.08, 1.0);
		Style.Window.VerticalMullions = 0;
		Style.Window.HorizontalMullions = 0;
		Style.Window.SillDepth = 0.08;
		Style.Window.SillThickness = 0.04;
		Style.Window.SillMaterial = TEXT("Grammar Flush Metal Sills");
		Style.Window.SillColor = FLinearColor(0.48, 0.5, 0.48, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.06;
		Style.Ledge.Height = 0.05;
		Style.Ledge.EveryNFloors = 3;
		Style.Ledge.Material = TEXT("Grammar Contemporary Shadow Joints");
		Style.Ledge.Color = FLinearColor(0.62, 0.62, 0.58, 1.0);

		Style.Balcony.bEnabled = true;
		Style.Balcony.Width = 2.8;
		Style.Balcony.Depth = 1.0;
		Style.Balcony.SlabHeight = 0.14;
		Style.Balcony.RailingHeight = 1.0;
		Style.Balcony.EveryNFloors = 1;
		Style.Balcony.Material = TEXT("Grammar Glass Balcony Railings");
		Style.Balcony.Color = FLinearColor(0.36, 0.46, 0.48, 0.86);
		Style.Balcony.RailingMaterial = TEXT("Grammar Laminated Glass Rails");
		Style.Balcony.RailingColor = FLinearColor(0.38, 0.5, 0.54, 0.78);
		Style.Balcony.RailingBarCount = 0;
		Style.Balcony.RailingBarWidth = 0.03;
		Style.Balcony.RailingBarDepth = 0.03;

		Style.Door = GrammarSubStyles::ResidentialDoorStyle(TEXT("Grammar Contemporary Entrance Glass"), FLinearColor(0.1, 0.18, 0.2, 0.9), TEXT("Grammar Contemporary Anthracite Door Frames"), FLinearColor(0.08, 0.085, 0.08, 1.0), /*bCanopyEnabled=*/true);
		Style.Antenna = GrammarSubStyles::DomesticAntennaStyle(EGrammarAntennaType::Radio, 1, 1.1);
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

	FFacadeStyleConfig GermanOfficeGridFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("german_office_grid");
		Style.WallMaterial = TEXT("Grammar Office Aluminum Grid");
		Style.WallColor = FLinearColor(0.22, 0.23, 0.24, 1.0);

		Style.Window.Width = 2.15;
		Style.Window.Height = 2.05;
		Style.Window.SillHeight = 0.45;
		Style.Window.Spacing = 2.45;
		Style.Window.MinMargin = 0.45;
		Style.Window.Depth = 0.025;
		Style.Window.Material = TEXT("Grammar Office Solar Glass");
		Style.Window.Color = FLinearColor(0.08, 0.22, 0.3, 0.9);
		Style.Window.FrameWidth = 0.055;
		Style.Window.FrameDepth = 0.035;
		Style.Window.FrameMaterial = TEXT("Grammar Office Dark Mullions");
		Style.Window.FrameColor = FLinearColor(0.08, 0.085, 0.09, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 1;
		Style.Window.SillDepth = 0.04;
		Style.Window.SillThickness = 0.03;
		Style.Window.SillMaterial = TEXT("Grammar Office Drip Edges");
		Style.Window.SillColor = FLinearColor(0.42, 0.44, 0.44, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.08;
		Style.Ledge.Height = 0.05;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Office Floor Bands");
		Style.Ledge.Color = FLinearColor(0.48, 0.5, 0.5, 1.0);

		Style.Balcony.bEnabled = false;

		Style.Door = GrammarSubStyles::OfficeDoorStyle();
		Style.Antenna = GrammarSubStyles::OfficeAntennaStyle(EGrammarAntennaType::OfficeCluster, 2, 3.4);
		return Style;
	}

	FFacadeStyleConfig KontorhausBrickOfficeFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("kontorhaus_brick_office");
		Style.WallMaterial = TEXT("Grammar Dark Brick Office");
		Style.WallColor = FLinearColor(0.38, 0.16, 0.11, 1.0);

		Style.Window.Width = 1.3;
		Style.Window.Height = 1.75;
		Style.Window.SillHeight = 0.75;
		Style.Window.Spacing = 2.2;
		Style.Window.MinMargin = 0.65;
		Style.Window.Depth = 0.06;
		Style.Window.Material = TEXT("Grammar Steel Office Windows");
		Style.Window.Color = FLinearColor(0.11, 0.18, 0.2, 1.0);
		Style.Window.FrameWidth = 0.09;
		Style.Window.FrameDepth = 0.045;
		Style.Window.FrameMaterial = TEXT("Grammar Kontorhaus Steel Frames");
		Style.Window.FrameColor = FLinearColor(0.06, 0.07, 0.07, 1.0);
		Style.Window.VerticalMullions = 1;
		Style.Window.HorizontalMullions = 2;
		Style.Window.SillDepth = 0.18;
		Style.Window.SillThickness = 0.06;
		Style.Window.SillMaterial = TEXT("Grammar Brick Office Sills");
		Style.Window.SillColor = FLinearColor(0.3, 0.11, 0.08, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.14;
		Style.Ledge.Height = 0.08;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Brick Expressionist Bands");
		Style.Ledge.Color = FLinearColor(0.28, 0.1, 0.07, 1.0);

		Style.Balcony.bEnabled = false;

		Style.Door = GrammarSubStyles::OfficeDoorStyle();
		Style.Antenna = GrammarSubStyles::OfficeAntennaStyle(EGrammarAntennaType::Broadcast, 1, 4.2);
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

	FFacadeStyleConfig CathedralStoneFacade()
	{
		FFacadeStyleConfig Style;
		Style.Name = TEXT("cathedral_stone");
		Style.BuildingValues = { TEXT("cathedral") };

		FGrammarStringList BuildingFilter;
		BuildingFilter.Values = { TEXT("cathedral") };
		FGrammarStringList AmenityFilter;
		AmenityFilter.Values = { TEXT("place_of_worship") };
		FGrammarStringList ReligionFilter;
		ReligionFilter.Values = { TEXT("christian") };
		FGrammarStringList HistoricFilter;
		HistoricFilter.Values = { TEXT("church"), TEXT("cathedral"), TEXT("yes"), TEXT("*") };
		Style.TagFilters.Add(TEXT("building"), BuildingFilter);
		Style.TagFilters.Add(TEXT("amenity"), AmenityFilter);
		Style.TagFilters.Add(TEXT("religion"), ReligionFilter);
		Style.TagFilters.Add(TEXT("historic"), HistoricFilter);

		Style.bHasDefaultLevels = true;
		Style.DefaultLevels = 5;
		Style.bHasDefaultFloorHeight = true;
		Style.DefaultFloorHeight = 5.2;

		Style.bOverrideRoof = true;
		Style.RoofOverride = FRoofStyleConfig();
		Style.RoofOverride.Type = EGrammarRoofType::Gabled;
		Style.RoofOverride.Height = 4.4;
		Style.RoofOverride.Overhang = 0.35;
		Style.RoofOverride.RidgeAlignment = EGrammarRidgeAlignment::LongestAxis;
		Style.RoofOverride.Material = TEXT("Grammar Cathedral Dark Slate Roof");
		Style.RoofOverride.Color = FLinearColor(0.075, 0.08, 0.09, 1.0);
		Style.RoofOverride.bEdgeEnabled = false;
		Style.RoofOverride.TileRows = 14;
		Style.RoofOverride.DormerCount = 0;
		Style.RoofOverride.RoofWindowCount = 0;
		Style.RoofOverride.ChimneyCount = 0;

		Style.WallMaterial = TEXT("Grammar Cathedral Carved Stone");
		Style.WallColor = FLinearColor(0.61, 0.59, 0.54, 1.0);
		Style.WallColorVariants = {
			FLinearColor(0.61, 0.59, 0.54, 1.0),
			FLinearColor(0.52, 0.51, 0.47, 1.0),
			FLinearColor(0.68, 0.65, 0.58, 1.0),
		};
		Style.WallColorVariantMode = EGrammarWallColorVariantMode::Facade;
		Style.WallRowColors = {
			FLinearColor(0.45, 0.43, 0.39, 1.0),
			FLinearColor(0.62, 0.6, 0.55, 1.0),
			FLinearColor(0.56, 0.54, 0.5, 1.0),
		};
		Style.WallRowColorMode = EGrammarWallRowColorMode::GroundAccent;

		Style.Window.Width = 1.35;
		Style.Window.Height = 4.4;
		Style.Window.SillHeight = 0.6;
		Style.Window.Spacing = 2.9;
		Style.Window.MinMargin = 1.05;
		Style.Window.Depth = 0.09;
		Style.Window.Material = TEXT("Grammar Cathedral Stained Glass");
		Style.Window.Color = FLinearColor(0.1, 0.16, 0.34, 0.92);
		Style.Window.FrameWidth = 0.16;
		Style.Window.FrameDepth = 0.075;
		Style.Window.FrameMaterial = TEXT("Grammar Cathedral Stone Tracery");
		Style.Window.FrameColor = FLinearColor(0.76, 0.73, 0.66, 1.0);
		Style.Window.VerticalMullions = 3;
		Style.Window.HorizontalMullions = 4;
		Style.Window.SillDepth = 0.3;
		Style.Window.SillThickness = 0.12;
		Style.Window.SillMaterial = TEXT("Grammar Cathedral Deep Stone Sills");
		Style.Window.SillColor = FLinearColor(0.72, 0.69, 0.62, 1.0);

		Style.Ledge.bEnabled = true;
		Style.Ledge.Depth = 0.28;
		Style.Ledge.Height = 0.14;
		Style.Ledge.EveryNFloors = 1;
		Style.Ledge.Material = TEXT("Grammar Cathedral String Courses");
		Style.Ledge.Color = FLinearColor(0.72, 0.69, 0.62, 1.0);

		Style.Balcony.bEnabled = false;

		Style.Door.bEnabled = true;
		Style.Door.Placement = EGrammarDoorPlacement::StreetFacing;
		Style.Door.Width = 3.2;
		Style.Door.Height = 4.35;
		Style.Door.Depth = 0.14;
		Style.Door.Material = TEXT("Grammar Cathedral Main Portal Doors");
		Style.Door.Color = FLinearColor(0.1, 0.052, 0.028, 1.0);
		Style.Door.FrameWidth = 0.32;
		Style.Door.FrameDepth = 0.12;
		Style.Door.FrameMaterial = TEXT("Grammar Cathedral Sculpted Portal");
		Style.Door.FrameColor = FLinearColor(0.76, 0.72, 0.62, 1.0);
		Style.Door.HandleRadius = 0.075;
		Style.Door.HandleMaterial = TEXT("Grammar Cathedral Iron Hardware");
		Style.Door.HandleColor = FLinearColor(0.04, 0.035, 0.03, 1.0);
		Style.Door.bCanopyEnabled = false;

		Style.Antenna = GrammarSubStyles::LightningRodStyle(3, 3.4);
		return Style;
	}

	FFacadeStyleConfig ApartmentGabledResidentialFacade()
	{
		// Port of grammar.py's apartment_gabled_residential_facade(), which is
		// gruenderzeit_residential_facade() with name/matching/roof overridden -- see that
		// function's own comment.
		FFacadeStyleConfig Style = GruenderzeitResidentialFacade();
		Style.Name = TEXT("apartment_gabled_residential");
		Style.BuildingValues = { TEXT("apartments") };

		FGrammarStringList BuildingFilter;
		BuildingFilter.Values = { TEXT("apartments") };
		FGrammarStringList ResidentialFilter;
		ResidentialFilter.Values = { TEXT("apartments") };
		FGrammarStringList BuildingUseFilter;
		BuildingUseFilter.Values = { TEXT("apartments"), TEXT("residential") };
		Style.TagFilters.Empty();
		Style.TagFilters.Add(TEXT("building"), BuildingFilter);
		Style.TagFilters.Add(TEXT("residential"), ResidentialFilter);
		Style.TagFilters.Add(TEXT("building:use"), BuildingUseFilter);

		Style.bOverrideRoof = true;
		Style.RoofOverride = FRoofStyleConfig();
		Style.RoofOverride.Type = EGrammarRoofType::Gabled;
		Style.RoofOverride.Height = 1.9;
		Style.RoofOverride.Overhang = 0.32;
		Style.RoofOverride.RidgeAlignment = EGrammarRidgeAlignment::ClosestStreet;
		Style.RoofOverride.Material = TEXT("Grammar Apartment Clay Tile Roof");
		Style.RoofOverride.Color = FLinearColor(0.35, 0.085, 0.05, 1.0);
		Style.RoofOverride.bEdgeEnabled = false;
		Style.RoofOverride.TileRows = 8;
		Style.RoofOverride.DormerCount = 2;
		Style.RoofOverride.RoofWindowCount = 2;
		Style.RoofOverride.ChimneyCount = 1;

		return Style;
	}

	FFacadeStyleConfig ApartmentPyramidResidentialFacade()
	{
		// Port of grammar.py's apartment_pyramid_residential_facade(), which is
		// contemporary_german_residential_facade() with name/matching/roof overridden.
		FFacadeStyleConfig Style = ContemporaryGermanResidentialFacade();
		Style.Name = TEXT("apartment_pyramid_residential");
		Style.BuildingValues = { TEXT("apartments") };

		FGrammarStringList BuildingFilter;
		BuildingFilter.Values = { TEXT("apartments") };
		FGrammarStringList ResidentialFilter;
		ResidentialFilter.Values = { TEXT("apartments") };
		FGrammarStringList BuildingUseFilter;
		BuildingUseFilter.Values = { TEXT("apartments"), TEXT("residential") };
		Style.TagFilters.Empty();
		Style.TagFilters.Add(TEXT("building"), BuildingFilter);
		Style.TagFilters.Add(TEXT("residential"), ResidentialFilter);
		Style.TagFilters.Add(TEXT("building:use"), BuildingUseFilter);

		Style.bOverrideRoof = true;
		Style.RoofOverride = FRoofStyleConfig();
		Style.RoofOverride.Type = EGrammarRoofType::Pyramid;
		Style.RoofOverride.Height = 1.45;
		Style.RoofOverride.Overhang = 0.28;
		Style.RoofOverride.RidgeAlignment = EGrammarRidgeAlignment::LongestAxis;
		Style.RoofOverride.Material = TEXT("Grammar Apartment Dark Tile Roof");
		Style.RoofOverride.Color = FLinearColor(0.18, 0.105, 0.075, 1.0);
		Style.RoofOverride.bEdgeEnabled = false;

		return Style;
	}

	TArray<FFacadeStyleConfig> NativeFacadeStyles()
	{
		return {
			StoneUrbanFacade(),
			QuietSideFacade(),
			BrickRowhouseFacade(),
			ModernGlassFacade(),
			SteelCurtainWallOfficeFacade(),
			ExposedSteelBraceOfficeFacade(),
			AtriumGlassTowerOfficeFacade(),
			OpenDeckParkingGarageFacade(),
			ConcreteMultistoreyParkingFacade(),
			TransitStopShelterFacade(),
			WarehouseFacade(),
			SupermarketFacade(),
			RetailShopfrontFacade(),
			IndustrialWarehouseFacade(),
			MediterraneanFacade(),
			GruenderzeitResidentialFacade(),
			BauhausResidentialFacade(),
			ContemporaryGermanResidentialFacade(),
			PlattenbauResidentialFacade(),
			GermanOfficeGridFacade(),
			KontorhausBrickOfficeFacade(),
			GothicChurchFacade(),
			CathedralStoneFacade(),
			ApartmentGabledResidentialFacade(),
			ApartmentPyramidResidentialFacade(),
		};
	}
}
