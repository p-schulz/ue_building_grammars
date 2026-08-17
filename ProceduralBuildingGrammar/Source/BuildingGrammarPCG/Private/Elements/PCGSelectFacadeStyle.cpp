#include "Elements/PCGSelectFacadeStyle.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadata.h"
#include "Grammar/GrammarStyleSelection.h"
#include "Config/BuildingGrammarConfig.h"
#include "PCGBuildingGrammarDefaults.h"

UPCGSelectFacadeStyleSettings::UPCGSelectFacadeStyleSettings()
{
	FBuildingGrammarConfig DefaultConfig;
	if (LoadDefaultGermanBuildingGrammarConfig(DefaultConfig))
	{
		Styles = MoveTemp(DefaultConfig.Styles);
		DefaultRoof = DefaultConfig.Roof;
	}
}

namespace
{
	const FName BuildingInfoPinLabel = TEXT("BuildingInfo");
	const FName StyleInfoPinLabel = TEXT("StyleInfo");

	// Meters (BuildingGrammarCore's own working unit) -> UE centimeters, matching the rest of this
	// pipeline's UE-world-units convention -- see the module's own header comment.
	constexpr double MetersToUnrealUnits = 100.0;

	// Reimplementation of GrammarEngineInternal::StyleHasShutters(StyleTokens(Style, {})) --
	// BuildingGrammarCore's own version is private to that module (Private/Grammar/
	// GrammarEngineInternal.h), not exported, so this is a self-contained port of just the keyword
	// check this node needs: does the style's own Name (tags are deliberately NOT consulted here --
	// classic calls StyleTokens with an empty Tags map for this specific check) contain one of these
	// era/regional keywords.
	bool StyleNameHasShutterKeyword(const FString& StyleName)
	{
		const FString Lower = StyleName.ToLower();
		static const TArray<FString> Keywords = {
			TEXT("fachwerk"), TEXT("mediterranean"), TEXT("rowhouse"), TEXT("reihenhaus"),
			TEXT("siedlung"), TEXT("gruenderzeit"), TEXT("jugendstil")
		};
		for (const FString& Keyword : Keywords)
		{
			if (Lower.Contains(Keyword))
			{
				return true;
			}
		}
		return false;
	}

	// Same enum-name-string convention as AntennaTypeToString below -- round-tripped back to the enum
	// by an equivalent reverse switch in UPCGRoofFrameGeneratorSettings/UPCGRoofDetailLayoutSettings/
	// UPCGRoofServiceAntennaLayoutSettings.
	FString RoofTypeToString(EGrammarRoofType Type)
	{
		switch (Type)
		{
		case EGrammarRoofType::Flat: return TEXT("Flat");
		case EGrammarRoofType::Gabled: return TEXT("Gabled");
		case EGrammarRoofType::Hipped: return TEXT("Hipped");
		case EGrammarRoofType::Pyramid: return TEXT("Pyramid");
		case EGrammarRoofType::Gambrel: return TEXT("Gambrel");
		case EGrammarRoofType::Mansard: return TEXT("Mansard");
		default: return TEXT("Flat");
		}
	}

	FString RidgeAlignmentToString(EGrammarRidgeAlignment Value)
	{
		switch (Value)
		{
		case EGrammarRidgeAlignment::ClosestStreet: return TEXT("ClosestStreet");
		case EGrammarRidgeAlignment::LongestAxis: return TEXT("LongestAxis");
		default: return TEXT("LongestAxis");
		}
	}

	FString DoorPlacementToString(EGrammarDoorPlacement Value)
	{
		switch (Value)
		{
		case EGrammarDoorPlacement::StreetFacing: return TEXT("StreetFacing");
		case EGrammarDoorPlacement::EachFacade: return TEXT("EachFacade");
		case EGrammarDoorPlacement::None: return TEXT("None");
		default: return TEXT("StreetFacing");
		}
	}

	FString WallColorVariantModeToString(EGrammarWallColorVariantMode Value)
	{
		switch (Value)
		{
		case EGrammarWallColorVariantMode::None: return TEXT("None");
		case EGrammarWallColorVariantMode::Cycle: return TEXT("Cycle");
		case EGrammarWallColorVariantMode::Building: return TEXT("Building");
		case EGrammarWallColorVariantMode::Facade: return TEXT("Facade");
		default: return TEXT("None");
		}
	}

	FString WallRowColorModeToString(EGrammarWallRowColorMode Value)
	{
		switch (Value)
		{
		case EGrammarWallRowColorMode::Cycle: return TEXT("Cycle");
		case EGrammarWallRowColorMode::GroundAccent: return TEXT("GroundAccent");
		default: return TEXT("Cycle");
		}
	}

	// PCG metadata attributes have no array type -- an FLinearColor array is serialized as a
	// semicolon-delimited "r,g,b,a;r,g,b,a;..." string, parsed back by an equivalent function in
	// UPCGExtrudeFootprintToWallsSettings (the only consumer -- see this node's own header comment
	// for why full variant/row-color resolution happens there, not here).
	FString SerializeColorArray(const TArray<FLinearColor>& Colors)
	{
		TArray<FString> Parts;
		Parts.Reserve(Colors.Num());
		for (const FLinearColor& Color : Colors)
		{
			Parts.Add(FString::Printf(TEXT("%f,%f,%f,%f"), Color.R, Color.G, Color.B, Color.A));
		}
		return FString::Join(Parts, TEXT(";"));
	}

	// Plain C++-enum-value-name strings (not the UMETA DisplayName, e.g. "Lightning Rod" -> this
	// returns "LightningRod") -- round-tripped back to the enum by an equivalent reverse switch in
	// UPCGRoofServiceAntennaLayoutSettings, so the exact spelling only needs to match between these
	// two switches, not any UI-facing text.
	FString AntennaTypeToString(EGrammarAntennaType Type)
	{
		switch (Type)
		{
		case EGrammarAntennaType::Tv: return TEXT("Tv");
		case EGrammarAntennaType::Radio: return TEXT("Radio");
		case EGrammarAntennaType::Satellite: return TEXT("Satellite");
		case EGrammarAntennaType::LightningRod: return TEXT("LightningRod");
		case EGrammarAntennaType::Cellular: return TEXT("Cellular");
		case EGrammarAntennaType::OfficeCluster: return TEXT("OfficeCluster");
		case EGrammarAntennaType::Broadcast: return TEXT("Broadcast");
		case EGrammarAntennaType::LampPost: return TEXT("LampPost");
		default: return TEXT("Tv");
		}
	}
}

TArray<FPCGPinProperties> UPCGSelectFacadeStyleSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(BuildingInfoPinLabel, EPCGDataType::Param);
	return Pins;
}

TArray<FPCGPinProperties> UPCGSelectFacadeStyleSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(StyleInfoPinLabel, EPCGDataType::Param);
	return Pins;
}

FPCGElementPtr UPCGSelectFacadeStyleSettings::CreateElement() const
{
	return MakeShared<FPCGSelectFacadeStyleElement>();
}

bool FPCGSelectFacadeStyleElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGSelectFacadeStyleSettings* Settings = Context->GetInputSettings<UPCGSelectFacadeStyleSettings>();
	check(Settings);

	static const FFacadeStyleConfig DefaultStyle;

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(BuildingInfoPinLabel);
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGParamData* BuildingInfo = Cast<UPCGParamData>(Input.Data.Get());
		if (!BuildingInfo)
		{
			continue;
		}

		const UPCGMetadata* InfoMetadata = BuildingInfo->ConstMetadata();
		const FPCGMetadataAttribute<FString>* SourceNameAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<FString>(TEXT("SourceName")) : nullptr;
		const FPCGMetadataAttribute<FString>* TagsJsonAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<FString>(TEXT("TagsJson")) : nullptr;
		if (!SourceNameAttr || !TagsJsonAttr)
		{
			continue;
		}

		UPCGParamData* StyleInfo = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
		UPCGMetadata* StyleMetadata = StyleInfo->MutableMetadata();
		FPCGMetadataAttribute<FString>* OutSourceNameAttr = StyleMetadata->CreateAttribute<FString>(TEXT("SourceName"), FString(), false, false);
		FPCGMetadataAttribute<FString>* StyleNameAttr = StyleMetadata->CreateAttribute<FString>(TEXT("StyleName"), FString(), false, false);
		FPCGMetadataAttribute<FString>* WallMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("WallMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* WallColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("WallColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<FString>* WallColorVariantsAttr = StyleMetadata->CreateAttribute<FString>(TEXT("WallColorVariants"), FString(), false, false);
		FPCGMetadataAttribute<FString>* WallColorVariantModeAttr = StyleMetadata->CreateAttribute<FString>(TEXT("WallColorVariantMode"), FString(), false, false);
		FPCGMetadataAttribute<FString>* WallRowColorsAttr = StyleMetadata->CreateAttribute<FString>(TEXT("WallRowColors"), FString(), false, false);
		FPCGMetadataAttribute<FString>* WallRowColorModeAttr = StyleMetadata->CreateAttribute<FString>(TEXT("WallRowColorMode"), FString(), false, false);
		FPCGMetadataAttribute<FString>* RoofTypeAttr = StyleMetadata->CreateAttribute<FString>(TEXT("RoofType"), FString(), false, false);
		FPCGMetadataAttribute<FString>* RidgeAlignmentAttr = StyleMetadata->CreateAttribute<FString>(TEXT("RidgeAlignment"), FString(), false, false);
		FPCGMetadataAttribute<FString>* RoofMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("RoofMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* RoofColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("RoofColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<double>* WindowWidthAttr = StyleMetadata->CreateAttribute<double>(TEXT("WindowWidth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* WindowHeightAttr = StyleMetadata->CreateAttribute<double>(TEXT("WindowHeight"), 0.0, false, false);
		FPCGMetadataAttribute<double>* WindowSpacingAttr = StyleMetadata->CreateAttribute<double>(TEXT("WindowSpacing"), 0.0, false, false);
		FPCGMetadataAttribute<double>* WindowMarginAttr = StyleMetadata->CreateAttribute<double>(TEXT("WindowMargin"), 0.0, false, false);
		FPCGMetadataAttribute<double>* WindowSillHeightAttr = StyleMetadata->CreateAttribute<double>(TEXT("WindowSillHeight"), 0.0, false, false);
		FPCGMetadataAttribute<double>* WindowDepthAttr = StyleMetadata->CreateAttribute<double>(TEXT("WindowDepth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* WindowRecessDepthAttr = StyleMetadata->CreateAttribute<double>(TEXT("WindowRecessDepth"), 0.0, false, false);
		FPCGMetadataAttribute<FString>* WindowMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("WindowMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* WindowColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("WindowColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<double>* WindowFrameWidthAttr = StyleMetadata->CreateAttribute<double>(TEXT("WindowFrameWidth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* WindowFrameDepthAttr = StyleMetadata->CreateAttribute<double>(TEXT("WindowFrameDepth"), 0.0, false, false);
		FPCGMetadataAttribute<FString>* WindowFrameMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("WindowFrameMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* WindowFrameColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("WindowFrameColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<int32>* WindowVerticalMullionsAttr = StyleMetadata->CreateAttribute<int32>(TEXT("WindowVerticalMullions"), 0, false, false);
		FPCGMetadataAttribute<int32>* WindowHorizontalMullionsAttr = StyleMetadata->CreateAttribute<int32>(TEXT("WindowHorizontalMullions"), 0, false, false);
		FPCGMetadataAttribute<double>* WindowSillDepthAttr = StyleMetadata->CreateAttribute<double>(TEXT("WindowSillDepth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* WindowSillThicknessAttr = StyleMetadata->CreateAttribute<double>(TEXT("WindowSillThickness"), 0.0, false, false);
		FPCGMetadataAttribute<FString>* WindowSillMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("WindowSillMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* WindowSillColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("WindowSillColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<bool>* DoorEnabledAttr = StyleMetadata->CreateAttribute<bool>(TEXT("DoorEnabled"), false, false, false);
		FPCGMetadataAttribute<double>* DoorWidthAttr = StyleMetadata->CreateAttribute<double>(TEXT("DoorWidth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* DoorHeightAttr = StyleMetadata->CreateAttribute<double>(TEXT("DoorHeight"), 0.0, false, false);
		FPCGMetadataAttribute<double>* DoorDepthAttr = StyleMetadata->CreateAttribute<double>(TEXT("DoorDepth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* DoorRecessDepthAttr = StyleMetadata->CreateAttribute<double>(TEXT("DoorRecessDepth"), 0.0, false, false);
		FPCGMetadataAttribute<FString>* DoorMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("DoorMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* DoorColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("DoorColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<FString>* DoorPlacementAttr = StyleMetadata->CreateAttribute<FString>(TEXT("DoorPlacement"), FString(), false, false);
		FPCGMetadataAttribute<double>* DoorFrameWidthAttr = StyleMetadata->CreateAttribute<double>(TEXT("DoorFrameWidth"), 0.0, false, false);

		FPCGMetadataAttribute<bool>* LedgeEnabledAttr = StyleMetadata->CreateAttribute<bool>(TEXT("LedgeEnabled"), false, false, false);
		FPCGMetadataAttribute<double>* LedgeDepthAttr = StyleMetadata->CreateAttribute<double>(TEXT("LedgeDepth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* LedgeHeightAttr = StyleMetadata->CreateAttribute<double>(TEXT("LedgeHeight"), 0.0, false, false);
		FPCGMetadataAttribute<int32>* LedgeEveryNFloorsAttr = StyleMetadata->CreateAttribute<int32>(TEXT("LedgeEveryNFloors"), 0, false, false);
		FPCGMetadataAttribute<FString>* LedgeMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("LedgeMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* LedgeColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("LedgeColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);

		FPCGMetadataAttribute<bool>* BalconyEnabledAttr = StyleMetadata->CreateAttribute<bool>(TEXT("BalconyEnabled"), false, false, false);
		FPCGMetadataAttribute<double>* BalconyWidthAttr = StyleMetadata->CreateAttribute<double>(TEXT("BalconyWidth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* BalconyDepthAttr = StyleMetadata->CreateAttribute<double>(TEXT("BalconyDepth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* BalconySlabHeightAttr = StyleMetadata->CreateAttribute<double>(TEXT("BalconySlabHeight"), 0.0, false, false);
		FPCGMetadataAttribute<double>* BalconyRailingHeightAttr = StyleMetadata->CreateAttribute<double>(TEXT("BalconyRailingHeight"), 0.0, false, false);
		FPCGMetadataAttribute<int32>* BalconyEveryNFloorsAttr = StyleMetadata->CreateAttribute<int32>(TEXT("BalconyEveryNFloors"), 0, false, false);
		FPCGMetadataAttribute<FString>* BalconyMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("BalconyMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* BalconyColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("BalconyColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<FString>* BalconyRailingMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("BalconyRailingMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* BalconyRailingColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("BalconyRailingColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<int32>* BalconyRailingBarCountAttr = StyleMetadata->CreateAttribute<int32>(TEXT("BalconyRailingBarCount"), 0, false, false);
		FPCGMetadataAttribute<double>* BalconyRailingBarWidthAttr = StyleMetadata->CreateAttribute<double>(TEXT("BalconyRailingBarWidth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* BalconyRailingBarDepthAttr = StyleMetadata->CreateAttribute<double>(TEXT("BalconyRailingBarDepth"), 0.0, false, false);

		FPCGMetadataAttribute<bool>* ShutterEnabledAttr = StyleMetadata->CreateAttribute<bool>(TEXT("ShutterEnabled"), false, false, false);

		FPCGMetadataAttribute<FString>* EdgeMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("EdgeMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* EdgeColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("EdgeColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<FString>* TileMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("TileMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* TileColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("TileColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<FString>* RoofWindowMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("RoofWindowMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* RoofWindowColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("RoofWindowColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<FString>* DormerMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("DormerMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* DormerColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("DormerColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<FString>* ChimneyMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("ChimneyMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* ChimneyColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("ChimneyColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);

		FPCGMetadataAttribute<bool>* AntennaEnabledAttr = StyleMetadata->CreateAttribute<bool>(TEXT("AntennaEnabled"), false, false, false);
		FPCGMetadataAttribute<FString>* AntennaTypeAttr = StyleMetadata->CreateAttribute<FString>(TEXT("AntennaType"), FString(), false, false);
		FPCGMetadataAttribute<int32>* AntennaCountAttr = StyleMetadata->CreateAttribute<int32>(TEXT("AntennaCount"), 0, false, false);
		FPCGMetadataAttribute<double>* AntennaMastHeightAttr = StyleMetadata->CreateAttribute<double>(TEXT("AntennaMastHeight"), 0.0, false, false);
		FPCGMetadataAttribute<double>* AntennaMastRadiusAttr = StyleMetadata->CreateAttribute<double>(TEXT("AntennaMastRadius"), 0.0, false, false);
		FPCGMetadataAttribute<double>* AntennaBaseWidthAttr = StyleMetadata->CreateAttribute<double>(TEXT("AntennaBaseWidth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* AntennaBaseDepthAttr = StyleMetadata->CreateAttribute<double>(TEXT("AntennaBaseDepth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* AntennaBaseHeightAttr = StyleMetadata->CreateAttribute<double>(TEXT("AntennaBaseHeight"), 0.0, false, false);
		FPCGMetadataAttribute<double>* AntennaPanelWidthAttr = StyleMetadata->CreateAttribute<double>(TEXT("AntennaPanelWidth"), 0.0, false, false);
		FPCGMetadataAttribute<double>* AntennaPanelHeightAttr = StyleMetadata->CreateAttribute<double>(TEXT("AntennaPanelHeight"), 0.0, false, false);
		FPCGMetadataAttribute<double>* AntennaPanelDepthAttr = StyleMetadata->CreateAttribute<double>(TEXT("AntennaPanelDepth"), 0.0, false, false);
		FPCGMetadataAttribute<FString>* AntennaMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("AntennaMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* AntennaColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("AntennaColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);
		FPCGMetadataAttribute<FString>* AntennaAccentMaterialAttr = StyleMetadata->CreateAttribute<FString>(TEXT("AntennaAccentMaterial"), FString(), false, false);
		FPCGMetadataAttribute<FVector4>* AntennaAccentColorAttr = StyleMetadata->CreateAttribute<FVector4>(TEXT("AntennaAccentColor"), FVector4(1.0, 1.0, 1.0, 1.0), false, false);

		const int64 Count = InfoMetadata->GetLocalItemCount();
		for (int64 EntryKey = 0; EntryKey < Count; ++EntryKey)
		{
			const FString SourceName = SourceNameAttr->GetValueFromItemKey(EntryKey);
			if (SourceName.IsEmpty())
			{
				continue;
			}

			const TMap<FString, FString> Tags = DeserializeTagsFromJson(TagsJsonAttr->GetValueFromItemKey(EntryKey));
			const TArray<const FFacadeStyleConfig*> Matches = FGrammarStyleSelection::SelectableStylesForTags(Tags, Settings->Styles);
			const FFacadeStyleConfig* Style = Matches.Num() > 0 ? Matches[0] : &DefaultStyle;

			const int64 OutEntryKey = StyleInfo->FindOrAddMetadataKey(FName(*SourceName));
			OutSourceNameAttr->SetValue(OutEntryKey, SourceName);
			StyleNameAttr->SetValue(OutEntryKey, Style->Name);
			WallMaterialAttr->SetValue(OutEntryKey, Style->WallMaterial);
			WallColorAttr->SetValue(OutEntryKey, FVector4(Style->WallColor.R, Style->WallColor.G, Style->WallColor.B, Style->WallColor.A));
			WallColorVariantsAttr->SetValue(OutEntryKey, SerializeColorArray(Style->WallColorVariants));
			WallColorVariantModeAttr->SetValue(OutEntryKey, WallColorVariantModeToString(Style->WallColorVariantMode));
			WallRowColorsAttr->SetValue(OutEntryKey, SerializeColorArray(Style->WallRowColors));
			WallRowColorModeAttr->SetValue(OutEntryKey, WallRowColorModeToString(Style->WallRowColorMode));

			// Mirrors BuildingGrammarEngine.cpp's own RoofStyleOverride-else-PrimaryStyle selection:
			// a style only ever supplies its own roof look if it opts in via bOverrideRoof, otherwise
			// every building sharing this config's root DefaultRoof.
			const FRoofStyleConfig& EffectiveRoof = Style->bOverrideRoof ? Style->RoofOverride : Settings->DefaultRoof;
			RoofTypeAttr->SetValue(OutEntryKey, RoofTypeToString(EffectiveRoof.Type));
			RidgeAlignmentAttr->SetValue(OutEntryKey, RidgeAlignmentToString(EffectiveRoof.RidgeAlignment));
			RoofMaterialAttr->SetValue(OutEntryKey, EffectiveRoof.Material);
			RoofColorAttr->SetValue(OutEntryKey, FVector4(EffectiveRoof.Color.R, EffectiveRoof.Color.G, EffectiveRoof.Color.B, EffectiveRoof.Color.A));

			const FWindowStyleConfig& Window = Style->Window;
			WindowWidthAttr->SetValue(OutEntryKey, Window.Width * MetersToUnrealUnits);
			WindowHeightAttr->SetValue(OutEntryKey, Window.Height * MetersToUnrealUnits);
			WindowSpacingAttr->SetValue(OutEntryKey, Window.Spacing * MetersToUnrealUnits);
			WindowMarginAttr->SetValue(OutEntryKey, Window.MinMargin * MetersToUnrealUnits);
			WindowSillHeightAttr->SetValue(OutEntryKey, Window.SillHeight * MetersToUnrealUnits);
			WindowDepthAttr->SetValue(OutEntryKey, Window.Depth * MetersToUnrealUnits);
			WindowRecessDepthAttr->SetValue(OutEntryKey, Window.RecessDepth * MetersToUnrealUnits);
			WindowMaterialAttr->SetValue(OutEntryKey, Window.Material);
			WindowColorAttr->SetValue(OutEntryKey, FVector4(Window.Color.R, Window.Color.G, Window.Color.B, Window.Color.A));
			WindowFrameWidthAttr->SetValue(OutEntryKey, Window.FrameWidth * MetersToUnrealUnits);
			WindowFrameDepthAttr->SetValue(OutEntryKey, Window.FrameDepth * MetersToUnrealUnits);
			WindowFrameMaterialAttr->SetValue(OutEntryKey, Window.FrameMaterial);
			WindowFrameColorAttr->SetValue(OutEntryKey, FVector4(Window.FrameColor.R, Window.FrameColor.G, Window.FrameColor.B, Window.FrameColor.A));
			WindowVerticalMullionsAttr->SetValue(OutEntryKey, Window.VerticalMullions);
			WindowHorizontalMullionsAttr->SetValue(OutEntryKey, Window.HorizontalMullions);
			WindowSillDepthAttr->SetValue(OutEntryKey, Window.SillDepth * MetersToUnrealUnits);
			WindowSillThicknessAttr->SetValue(OutEntryKey, Window.SillThickness * MetersToUnrealUnits);
			WindowSillMaterialAttr->SetValue(OutEntryKey, Window.SillMaterial);
			WindowSillColorAttr->SetValue(OutEntryKey, FVector4(Window.SillColor.R, Window.SillColor.G, Window.SillColor.B, Window.SillColor.A));

			const FDoorStyleConfig& Door = Style->Door;
			DoorEnabledAttr->SetValue(OutEntryKey, Door.bEnabled);
			DoorWidthAttr->SetValue(OutEntryKey, Door.Width * MetersToUnrealUnits);
			DoorHeightAttr->SetValue(OutEntryKey, Door.Height * MetersToUnrealUnits);
			DoorDepthAttr->SetValue(OutEntryKey, Door.Depth * MetersToUnrealUnits);
			DoorRecessDepthAttr->SetValue(OutEntryKey, Door.RecessDepth * MetersToUnrealUnits);
			DoorMaterialAttr->SetValue(OutEntryKey, Door.Material);
			DoorColorAttr->SetValue(OutEntryKey, FVector4(Door.Color.R, Door.Color.G, Door.Color.B, Door.Color.A));
			DoorPlacementAttr->SetValue(OutEntryKey, DoorPlacementToString(Door.Placement));
			DoorFrameWidthAttr->SetValue(OutEntryKey, Door.FrameWidth * MetersToUnrealUnits);

			const FLedgeStyleConfig& Ledge = Style->Ledge;
			LedgeEnabledAttr->SetValue(OutEntryKey, Ledge.bEnabled);
			LedgeDepthAttr->SetValue(OutEntryKey, Ledge.Depth * MetersToUnrealUnits);
			LedgeHeightAttr->SetValue(OutEntryKey, Ledge.Height * MetersToUnrealUnits);
			LedgeEveryNFloorsAttr->SetValue(OutEntryKey, Ledge.EveryNFloors);
			LedgeMaterialAttr->SetValue(OutEntryKey, Ledge.Material);
			LedgeColorAttr->SetValue(OutEntryKey, FVector4(Ledge.Color.R, Ledge.Color.G, Ledge.Color.B, Ledge.Color.A));

			const FBalconyStyleConfig& Balcony = Style->Balcony;
			BalconyEnabledAttr->SetValue(OutEntryKey, Balcony.bEnabled);
			BalconyWidthAttr->SetValue(OutEntryKey, Balcony.Width * MetersToUnrealUnits);
			BalconyDepthAttr->SetValue(OutEntryKey, Balcony.Depth * MetersToUnrealUnits);
			BalconySlabHeightAttr->SetValue(OutEntryKey, Balcony.SlabHeight * MetersToUnrealUnits);
			BalconyRailingHeightAttr->SetValue(OutEntryKey, Balcony.RailingHeight * MetersToUnrealUnits);
			BalconyEveryNFloorsAttr->SetValue(OutEntryKey, Balcony.EveryNFloors);
			BalconyMaterialAttr->SetValue(OutEntryKey, Balcony.Material);
			BalconyColorAttr->SetValue(OutEntryKey, FVector4(Balcony.Color.R, Balcony.Color.G, Balcony.Color.B, Balcony.Color.A));
			BalconyRailingMaterialAttr->SetValue(OutEntryKey, Balcony.RailingMaterial);
			BalconyRailingColorAttr->SetValue(OutEntryKey, FVector4(Balcony.RailingColor.R, Balcony.RailingColor.G, Balcony.RailingColor.B, Balcony.RailingColor.A));
			BalconyRailingBarCountAttr->SetValue(OutEntryKey, Balcony.RailingBarCount);
			BalconyRailingBarWidthAttr->SetValue(OutEntryKey, Balcony.RailingBarWidth * MetersToUnrealUnits);
			BalconyRailingBarDepthAttr->SetValue(OutEntryKey, Balcony.RailingBarDepth * MetersToUnrealUnits);

			ShutterEnabledAttr->SetValue(OutEntryKey, StyleNameHasShutterKeyword(Style->Name));

			EdgeMaterialAttr->SetValue(OutEntryKey, EffectiveRoof.EdgeMaterial);
			EdgeColorAttr->SetValue(OutEntryKey, FVector4(EffectiveRoof.EdgeColor.R, EffectiveRoof.EdgeColor.G, EffectiveRoof.EdgeColor.B, EffectiveRoof.EdgeColor.A));
			TileMaterialAttr->SetValue(OutEntryKey, EffectiveRoof.TileMaterial);
			TileColorAttr->SetValue(OutEntryKey, FVector4(EffectiveRoof.TileColor.R, EffectiveRoof.TileColor.G, EffectiveRoof.TileColor.B, EffectiveRoof.TileColor.A));
			RoofWindowMaterialAttr->SetValue(OutEntryKey, EffectiveRoof.RoofWindowMaterial);
			RoofWindowColorAttr->SetValue(OutEntryKey, FVector4(EffectiveRoof.RoofWindowColor.R, EffectiveRoof.RoofWindowColor.G, EffectiveRoof.RoofWindowColor.B, EffectiveRoof.RoofWindowColor.A));
			DormerMaterialAttr->SetValue(OutEntryKey, EffectiveRoof.DormerMaterial);
			DormerColorAttr->SetValue(OutEntryKey, FVector4(EffectiveRoof.DormerColor.R, EffectiveRoof.DormerColor.G, EffectiveRoof.DormerColor.B, EffectiveRoof.DormerColor.A));
			ChimneyMaterialAttr->SetValue(OutEntryKey, EffectiveRoof.ChimneyMaterial);
			ChimneyColorAttr->SetValue(OutEntryKey, FVector4(EffectiveRoof.ChimneyColor.R, EffectiveRoof.ChimneyColor.G, EffectiveRoof.ChimneyColor.B, EffectiveRoof.ChimneyColor.A));

			const FAntennaStyleConfig& Antenna = Style->Antenna;
			AntennaEnabledAttr->SetValue(OutEntryKey, Antenna.bEnabled);
			AntennaTypeAttr->SetValue(OutEntryKey, AntennaTypeToString(Antenna.Type));
			AntennaCountAttr->SetValue(OutEntryKey, Antenna.Count);
			AntennaMastHeightAttr->SetValue(OutEntryKey, Antenna.MastHeight * MetersToUnrealUnits);
			AntennaMastRadiusAttr->SetValue(OutEntryKey, Antenna.MastRadius * MetersToUnrealUnits);
			AntennaBaseWidthAttr->SetValue(OutEntryKey, Antenna.BaseWidth * MetersToUnrealUnits);
			AntennaBaseDepthAttr->SetValue(OutEntryKey, Antenna.BaseDepth * MetersToUnrealUnits);
			AntennaBaseHeightAttr->SetValue(OutEntryKey, Antenna.BaseHeight * MetersToUnrealUnits);
			AntennaPanelWidthAttr->SetValue(OutEntryKey, Antenna.PanelWidth * MetersToUnrealUnits);
			AntennaPanelHeightAttr->SetValue(OutEntryKey, Antenna.PanelHeight * MetersToUnrealUnits);
			AntennaPanelDepthAttr->SetValue(OutEntryKey, Antenna.PanelDepth * MetersToUnrealUnits);
			AntennaMaterialAttr->SetValue(OutEntryKey, Antenna.Material);
			AntennaColorAttr->SetValue(OutEntryKey, FVector4(Antenna.Color.R, Antenna.Color.G, Antenna.Color.B, Antenna.Color.A));
			AntennaAccentMaterialAttr->SetValue(OutEntryKey, Antenna.AccentMaterial);
			AntennaAccentColorAttr->SetValue(OutEntryKey, FVector4(Antenna.AccentColor.R, Antenna.AccentColor.G, Antenna.AccentColor.B, Antenna.AccentColor.A));
		}

		FPCGTaggedData& StyleOut = Context->OutputData.TaggedData.Emplace_GetRef();
		StyleOut.Data = StyleInfo;
		StyleOut.Pin = StyleInfoPinLabel;
		StyleOut.Tags = Input.Tags;
	}

	return true;
}
