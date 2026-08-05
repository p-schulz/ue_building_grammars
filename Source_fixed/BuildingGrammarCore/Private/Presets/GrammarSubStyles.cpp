#include "Presets/GrammarSubStyles.h"

namespace GrammarSubStyles
{
	FDoorStyleConfig ResidentialDoorStyle(const FString& Material, const FLinearColor& Color, const FString& FrameMaterial, const FLinearColor& FrameColor, bool bCanopyEnabled)
	{
		FDoorStyleConfig Door;
		Door.bEnabled = true;
		Door.Placement = EGrammarDoorPlacement::StreetFacing;
		Door.Width = 1.25;
		Door.Height = 2.35;
		Door.Depth = 0.09;
		Door.Material = Material;
		Door.Color = Color;
		Door.FrameWidth = 0.13;
		Door.FrameDepth = 0.05;
		Door.FrameMaterial = FrameMaterial;
		Door.FrameColor = FrameColor;
		Door.HandleRadius = 0.045;
		Door.HandleColor = FLinearColor(0.84, 0.68, 0.32, 1.0);
		Door.bCanopyEnabled = bCanopyEnabled;
		Door.CanopyWidth = 1.9;
		Door.CanopyDepth = 0.7;
		Door.CanopyThickness = 0.08;
		Door.CanopyMaterial = TEXT("Grammar Residential Door Canopy");
		Door.CanopyColor = FLinearColor(0.36, 0.36, 0.34, 1.0);
		return Door;
	}

	FDoorStyleConfig OfficeDoorStyle()
	{
		FDoorStyleConfig Door;
		Door.bEnabled = true;
		Door.Placement = EGrammarDoorPlacement::StreetFacing;
		Door.Width = 2.2;
		Door.Height = 3.1;
		Door.Depth = 0.06;
		Door.Material = TEXT("Grammar Office Entrance Glass");
		Door.Color = FLinearColor(0.1, 0.2, 0.25, 0.88);
		Door.FrameWidth = 0.09;
		Door.FrameDepth = 0.045;
		Door.FrameMaterial = TEXT("Grammar Office Entrance Frames");
		Door.FrameColor = FLinearColor(0.08, 0.085, 0.09, 1.0);
		Door.HandleRadius = 0.04;
		Door.HandleMaterial = TEXT("Grammar Stainless Door Handles");
		Door.HandleColor = FLinearColor(0.75, 0.74, 0.68, 1.0);
		Door.bCanopyEnabled = true;
		Door.CanopyWidth = 3.0;
		Door.CanopyDepth = 1.0;
		Door.CanopyThickness = 0.1;
		Door.CanopyMaterial = TEXT("Grammar Office Entrance Canopy");
		Door.CanopyColor = FLinearColor(0.18, 0.19, 0.19, 1.0);
		return Door;
	}

	FDoorStyleConfig ShopfrontDoorStyle(const FString& Material, const FLinearColor& Color, const FString& FrameMaterial, const FLinearColor& FrameColor)
	{
		FDoorStyleConfig Door;
		Door.bEnabled = true;
		Door.Placement = EGrammarDoorPlacement::StreetFacing;
		Door.Width = 2.55;
		Door.Height = 3.0;
		Door.Depth = 0.06;
		Door.Material = Material;
		Door.Color = Color;
		Door.FrameWidth = 0.08;
		Door.FrameDepth = 0.045;
		Door.FrameMaterial = FrameMaterial;
		Door.FrameColor = FrameColor;
		Door.HandleRadius = 0.045;
		Door.HandleMaterial = TEXT("Grammar Shopfront Pull Handles");
		Door.HandleColor = FLinearColor(0.72, 0.72, 0.66, 1.0);
		Door.bCanopyEnabled = true;
		Door.CanopyWidth = 3.4;
		Door.CanopyDepth = 0.9;
		Door.CanopyThickness = 0.09;
		Door.CanopyMaterial = TEXT("Grammar Shopfront Awning Edge");
		Door.CanopyColor = FLinearColor(0.18, 0.18, 0.16, 1.0);
		return Door;
	}

	FAntennaStyleConfig DomesticAntennaStyle(EGrammarAntennaType Kind, int32 Count, double MastHeight)
	{
		FAntennaStyleConfig Antenna;
		Antenna.bEnabled = true;
		Antenna.Type = Kind;
		Antenna.Count = Count;
		Antenna.MastHeight = MastHeight;
		Antenna.MastRadius = 0.025;
		Antenna.BaseWidth = 0.28;
		Antenna.BaseDepth = 0.28;
		Antenna.BaseHeight = 0.12;
		Antenna.PanelWidth = 0.45;
		Antenna.PanelHeight = 0.18;
		Antenna.PanelDepth = 0.035;
		Antenna.Material = TEXT("Grammar Domestic Antenna Metal");
		Antenna.Color = FLinearColor(0.32, 0.32, 0.31, 1.0);
		Antenna.AccentMaterial = TEXT("Grammar Domestic Antenna Elements");
		Antenna.AccentColor = FLinearColor(0.74, 0.74, 0.68, 1.0);
		return Antenna;
	}

	FAntennaStyleConfig SatelliteAntennaStyle(int32 Count, double MastHeight)
	{
		FAntennaStyleConfig Antenna;
		Antenna.bEnabled = true;
		Antenna.Type = EGrammarAntennaType::Satellite;
		Antenna.Count = Count;
		Antenna.MastHeight = MastHeight;
		Antenna.MastRadius = 0.025;
		Antenna.BaseWidth = 0.32;
		Antenna.BaseDepth = 0.32;
		Antenna.BaseHeight = 0.12;
		Antenna.PanelWidth = 0.55;
		Antenna.PanelHeight = 0.55;
		Antenna.PanelDepth = 0.05;
		Antenna.Material = TEXT("Grammar Satellite Mount Metal");
		Antenna.Color = FLinearColor(0.3, 0.3, 0.29, 1.0);
		Antenna.AccentMaterial = TEXT("Grammar Pale Satellite Dish");
		Antenna.AccentColor = FLinearColor(0.82, 0.82, 0.76, 1.0);
		return Antenna;
	}

	FAntennaStyleConfig LightningRodStyle(int32 Count, double MastHeight)
	{
		FAntennaStyleConfig Antenna;
		Antenna.bEnabled = true;
		Antenna.Type = EGrammarAntennaType::LightningRod;
		Antenna.Count = Count;
		Antenna.MastHeight = MastHeight;
		Antenna.MastRadius = 0.018;
		Antenna.BaseWidth = 0.18;
		Antenna.BaseDepth = 0.18;
		Antenna.BaseHeight = 0.08;
		Antenna.PanelWidth = 0.12;
		Antenna.PanelHeight = 0.35;
		Antenna.PanelDepth = 0.02;
		Antenna.Material = TEXT("Grammar Lightning Rod Steel");
		Antenna.Color = FLinearColor(0.42, 0.42, 0.39, 1.0);
		Antenna.AccentMaterial = TEXT("Grammar Lightning Rod Tip");
		Antenna.AccentColor = FLinearColor(0.76, 0.74, 0.66, 1.0);
		return Antenna;
	}

	FAntennaStyleConfig OfficeAntennaStyle(EGrammarAntennaType Kind, int32 Count, double MastHeight)
	{
		FAntennaStyleConfig Antenna;
		Antenna.bEnabled = true;
		Antenna.Type = Kind;
		Antenna.Count = Count;
		Antenna.MastHeight = MastHeight;
		Antenna.MastRadius = 0.06;
		Antenna.BaseWidth = 0.8;
		Antenna.BaseDepth = 0.65;
		Antenna.BaseHeight = 0.28;
		Antenna.PanelWidth = 0.42;
		Antenna.PanelHeight = 1.25;
		Antenna.PanelDepth = 0.08;
		Antenna.Material = TEXT("Grammar Communication Mast Steel");
		Antenna.Color = FLinearColor(0.28, 0.29, 0.29, 1.0);
		Antenna.AccentMaterial = TEXT("Grammar Cellular Antenna Panels");
		Antenna.AccentColor = FLinearColor(0.82, 0.82, 0.76, 1.0);
		return Antenna;
	}

	FAntennaStyleConfig ParkingLampStyle(int32 Count, double MastHeight)
	{
		FAntennaStyleConfig Antenna;
		Antenna.bEnabled = true;
		Antenna.Type = EGrammarAntennaType::LampPost;
		Antenna.Count = Count;
		Antenna.MastHeight = MastHeight;
		Antenna.MastRadius = 0.04;
		Antenna.BaseWidth = 0.28;
		Antenna.BaseDepth = 0.28;
		Antenna.BaseHeight = 0.12;
		Antenna.PanelWidth = 0.55;
		Antenna.PanelHeight = 0.12;
		Antenna.PanelDepth = 0.18;
		Antenna.Material = TEXT("Grammar Parking Lamp Posts");
		Antenna.Color = FLinearColor(0.16, 0.17, 0.17, 1.0);
		Antenna.AccentMaterial = TEXT("Grammar Parking LED Lamp Heads");
		Antenna.AccentColor = FLinearColor(1.0, 0.92, 0.62, 1.0);
		return Antenna;
	}
}
