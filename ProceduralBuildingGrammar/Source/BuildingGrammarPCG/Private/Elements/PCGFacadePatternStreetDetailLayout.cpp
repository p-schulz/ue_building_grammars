#include "Elements/PCGFacadePatternStreetDetailLayout.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Metadata/PCGMetadata.h"
#include "GrammarKitResolver.h"
#include "PCGBuildingGrammarDefaults.h"
#include "UObject/SoftObjectPath.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const FName EdgesPinLabel = TEXT("Edges");
	const FName BuildingInfoPinLabel = TEXT("BuildingInfo");
	const FName StyleInfoPinLabel = TEXT("StyleInfo");
	const FName StreetsPinLabel = TEXT("Streets");
	const FName PlacementsPinLabel = TEXT("Placements");

	// Same narrow substring-blob approximation as UPCGRoofServiceAntennaLayoutSettings -- see that
	// class's header comment.
	FString BuildTokenBlob(const FString& StyleName, const TMap<FString, FString>& Tags)
	{
		FString Blob = StyleName;
		for (const TCHAR* Key : { TEXT("building"), TEXT("building:use"), TEXT("shop"), TEXT("office"), TEXT("industrial"), TEXT("landuse"), TEXT("amenity"), TEXT("parking") })
		{
			if (const FString* Value = Tags.Find(Key))
			{
				Blob += TEXT(" ") + *Value;
			}
		}
		return Blob.ToLower();
	}

	bool BlobHasAny(const FString& Blob, const TArray<FString>& Keywords)
	{
		for (const FString& Keyword : Keywords)
		{
			if (Blob.Contains(Keyword))
			{
				return true;
			}
		}
		return false;
	}
}

TArray<FPCGPinProperties> UPCGFacadePatternStreetDetailLayoutSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(EdgesPinLabel, EPCGDataType::Point);
	Pins.Emplace(BuildingInfoPinLabel, EPCGDataType::Param);
	Pins.Emplace(StyleInfoPinLabel, EPCGDataType::Param);
	// Not required -- entirely unused if unconnected, falling back to the explicit-tag-only stub
	// (see this class's header comment).
	Pins.Emplace(StreetsPinLabel, EPCGDataType::Spline);
	return Pins;
}

TArray<FPCGPinProperties> UPCGFacadePatternStreetDetailLayoutSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(PlacementsPinLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGFacadePatternStreetDetailLayoutSettings::CreateElement() const
{
	return MakeShared<FPCGFacadePatternStreetDetailLayoutElement>();
}

bool FPCGFacadePatternStreetDetailLayoutElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGFacadePatternStreetDetailLayoutSettings* Settings = Context->GetInputSettings<UPCGFacadePatternStreetDetailLayoutSettings>();
	check(Settings);

	const UPCGParamData* StyleInfo = nullptr;
	for (const FPCGTaggedData& StyleData : Context->InputData.GetInputsByPin(StyleInfoPinLabel))
	{
		if (const UPCGParamData* Param = Cast<UPCGParamData>(StyleData.Data.Get()))
		{
			StyleInfo = Param;
			break;
		}
	}
	const UPCGMetadata* StyleMetadata = StyleInfo ? StyleInfo->ConstMetadata() : nullptr;
	const FPCGMetadataAttribute<FString>* StyleNameAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("StyleName")) : nullptr;

	const UPCGParamData* BuildingInfo = nullptr;
	for (const FPCGTaggedData& InfoData : Context->InputData.GetInputsByPin(BuildingInfoPinLabel))
	{
		if (const UPCGParamData* Param = Cast<UPCGParamData>(InfoData.Data.Get()))
		{
			BuildingInfo = Param;
			break;
		}
	}
	const UPCGMetadata* InfoMetadata = BuildingInfo ? BuildingInfo->ConstMetadata() : nullptr;
	const FPCGMetadataAttribute<FString>* TagsJsonAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<FString>(TEXT("TagsJson")) : nullptr;
	const FPCGMetadataAttribute<int32>* LevelsAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<int32>(TEXT("Levels")) : nullptr;
	const FPCGMetadataAttribute<double>* TotalHeightAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<double>(TEXT("TotalHeight")) : nullptr;

	// Gathered once (not per building) -- the street network is shared graph-wide, not per-building
	// data. Every "Streets" entry contributes its own consecutive-spline-point segments; unconnected
	// or empty just means every building falls back to the explicit-tag-only stub (see this class's
	// header comment).
	TArray<FStreetSegment> StreetSegments;
	for (const FPCGTaggedData& StreetData : Context->InputData.GetInputsByPin(StreetsPinLabel))
	{
		const UPCGSplineData* StreetSpline = Cast<UPCGSplineData>(StreetData.Data.Get());
		if (!StreetSpline)
		{
			continue;
		}
		const FString StreetName = ExtractStreetNameFromTags(StreetData.Tags);
		const TArray<FSplinePoint> StreetPoints = StreetSpline->GetSplinePoints();
		for (int32 Index = 0; Index + 1 < StreetPoints.Num(); ++Index)
		{
			FStreetSegment& Segment = StreetSegments.AddDefaulted_GetRef();
			Segment.Start = StreetPoints[Index].Position;
			Segment.End = StreetPoints[Index + 1].Position;
			Segment.Name = StreetName;
		}
	}

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(EdgesPinLabel);
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGPointData* EdgeData = Cast<UPCGPointData>(Input.Data.Get());
		if (!EdgeData)
		{
			continue;
		}

		const UPCGMetadata* EdgeMetadata = EdgeData->ConstMetadata();
		const FPCGMetadataAttribute<double>* LengthAttr = EdgeMetadata ? EdgeMetadata->GetConstTypedAttribute<double>(TEXT("Length")) : nullptr;
		const TArray<FPCGPoint>& EdgePoints = EdgeData->GetPoints();
		if (EdgePoints.Num() == 0 || !LengthAttr)
		{
			continue;
		}

		const FString SourceName = ExtractSourceNameFromTags(Input.Tags);

		const int64 InfoEntryKey = BuildingInfo ? BuildingInfo->FindMetadataKey(FName(*SourceName)) : INDEX_NONE;
		TMap<FString, FString> Tags;
		if (InfoEntryKey != INDEX_NONE && TagsJsonAttr)
		{
			Tags = DeserializeTagsFromJson(TagsJsonAttr->GetValueFromItemKey(InfoEntryKey));
		}

		// This building's own OSM-derived Levels/TotalHeight (see UPCGLoadOsmBuildingVolumesSettings'
		// header comment) if BuildingInfo has a usable row, else this node's own flat FloorCount/
		// FloorHeight -- see this class's header comment.
		int32 EffectiveFloorCount = Settings->FloorCount;
		double EffectiveFloorHeight = Settings->FloorHeight;
		if (InfoEntryKey != INDEX_NONE && LevelsAttr && TotalHeightAttr)
		{
			const int32 Levels = LevelsAttr->GetValueFromItemKey(InfoEntryKey);
			const double TotalHeightValue = TotalHeightAttr->GetValueFromItemKey(InfoEntryKey);
			if (Levels > 0 && TotalHeightValue > 0.0)
			{
				EffectiveFloorCount = Levels;
				EffectiveFloorHeight = TotalHeightValue / Levels;
			}
		}
		const double TotalHeight = EffectiveFloorCount * EffectiveFloorHeight;

		FString StyleName;
		if (StyleInfo && StyleNameAttr)
		{
			const int64 StyleEntryKey = StyleInfo->FindMetadataKey(FName(*SourceName));
			if (StyleEntryKey != INDEX_NONE)
			{
				StyleName = StyleNameAttr->GetValueFromItemKey(StyleEntryKey);
			}
		}
		const FString TokenBlob = BuildTokenBlob(StyleName, Tags);

		const FString* ShopTag = Tags.Find(TEXT("shop"));
		const FString* IndustrialTag = Tags.Find(TEXT("industrial"));
		const bool bIsRetail = (ShopTag && !ShopTag->IsEmpty()) || BlobHasAny(TokenBlob, { TEXT("retail"), TEXT("shopfront"), TEXT("shop"), TEXT("supermarket"), TEXT("commercial") });
		const bool bIsIndustrial = (IndustrialTag && !IndustrialTag->IsEmpty()) || BlobHasAny(TokenBlob, { TEXT("industrial"), TEXT("warehouse"), TEXT("factory"), TEXT("logistics"), TEXT("manufacturing") });
		const bool bIsParking = BlobHasAny(TokenBlob, { TEXT("parking"), TEXT("garage"), TEXT("multistorey"), TEXT("car") });
		const bool bStairCoreTokens = BlobHasAny(TokenBlob, { TEXT("office"), TEXT("parking"), TEXT("plattenbau"), TEXT("apartment"), TEXT("apartments"), TEXT("residential"), TEXT("modern") });

		const int32 StreetSideIndex = DetermineStreetFacingSideIndex(Tags, EdgePoints, LengthAttr, StreetSegments, Settings->StreetSearchRadius);

		UPCGPointData* PlacementData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		UPCGMetadata* PlacementMetadata = PlacementData->MutableMetadata();
		FPCGMetadataAttribute<FString>* RoleAttr = PlacementMetadata->CreateAttribute<FString>(TEXT("Role"), FString(), false, false);
		FPCGMetadataAttribute<double>* WidthAttr = PlacementMetadata->CreateAttribute<double>(TEXT("Width"), 0.0, false, false);
		FPCGMetadataAttribute<double>* HeightAttr = PlacementMetadata->CreateAttribute<double>(TEXT("Height"), 0.0, false, false);
		FPCGMetadataAttribute<FSoftObjectPath>* MaterialOverrideAttr = PlacementMetadata->CreateAttribute<FSoftObjectPath>(TEXT("MaterialOverride"), FSoftObjectPath(), false, false);

		// Same box CENTER + (Width,Depth,Height) Scale + Depth*0.5-outward-push convention as
		// UPCGFacadeWindowDoorLayoutSettings' own AddPlacement -- see its header comment. Every role
		// in this node follows classic's own PointOnSegment(..., Depth/2) convention (verified against
		// every other classic placement function in this codebase; GrammarFacadeDepth.cpp doesn't
		// spell out Center explicitly for the pattern-band roles, but there is no other convention
		// used anywhere else in the classic engine).
		auto AddPlacement = [&](const FTransform& EdgeTransform, double OffsetAlongEdge, double Width, double Height, double Depth, const TCHAR* Role, double BottomZ, UMaterialInterface* Material)
		{
			FVector WorldCenter = EdgeTransform.TransformPosition(FVector(OffsetAlongEdge, Depth * 0.5, 0.0));
			WorldCenter.Z += BottomZ + Height * 0.5;

			FPCGPoint Point;
			Point.Transform = FTransform(EdgeTransform.GetRotation(), WorldCenter, FVector(Width, Depth, Height));
			Point.Density = 1.0f;
			Point.MetadataEntry = PlacementMetadata->AddEntry();
			RoleAttr->SetValue(Point.MetadataEntry, FString(Role));
			WidthAttr->SetValue(Point.MetadataEntry, Width);
			HeightAttr->SetValue(Point.MetadataEntry, Height);
			if (Material)
			{
				MaterialOverrideAttr->SetValue(Point.MetadataEntry, FSoftObjectPath(Material));
			}
			PlacementData->GetMutablePoints().Add(Point);
		};

		for (int32 EdgeIndex = 0; EdgeIndex < EdgePoints.Num(); ++EdgeIndex)
		{
			const FPCGPoint& EdgePoint = EdgePoints[EdgeIndex];
			const double Length = LengthAttr->GetValueFromItemKey(EdgePoint.MetadataEntry);
			if (Length <= 10.0)
			{
				continue;
			}
			const FTransform& EdgeTransform = EdgePoint.Transform;
			const bool bStreetFacing = (EdgeIndex == StreetSideIndex);

			// ---- Facade pattern (FacadePatternPlacements) -- every edge, purely token-gated ----
			if (BlobHasAny(TokenBlob, { TEXT("plattenbau"), TEXT("prefab"), TEXT("industrial"), TEXT("warehouse"), TEXT("parking"), TEXT("office"), TEXT("curtain") }))
			{
				UMaterialInterface* SeamMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("panel_seam"), TEXT("Grammar Facade Panel Seams"), FLinearColor(0.2, 0.205, 0.2, 1.0));
				const int32 VerticalCount = FMath::Clamp(FMath::FloorToInt(Length / 320.0), 1, 8);
				for (int32 Index = 1; Index <= VerticalCount; ++Index)
				{
					const double Offset = Length * Index / static_cast<double>(VerticalCount + 1);
					AddPlacement(EdgeTransform, Offset, 4.5, TotalHeight, 4.5, TEXT("panel_seam"), 0.0, SeamMaterial);
				}
				for (int32 FloorIndex = 1; FloorIndex < EffectiveFloorCount; ++FloorIndex)
				{
					const double FloorBottom = FloorIndex * EffectiveFloorHeight;
					AddPlacement(EdgeTransform, Length * 0.5, Length, 5.0, 4.5, TEXT("panel_seam"), FloorBottom - 2.5, SeamMaterial);
				}
			}

			if (BlobHasAny(TokenBlob, { TEXT("passivhaus"), TEXT("contemporary"), TEXT("modern"), TEXT("bauhaus") }))
			{
				UMaterialInterface* InsulationMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("insulation_band"), TEXT("Grammar Insulation Shadow Bands"), FLinearColor(0.78, 0.78, 0.72, 1.0));
				for (int32 FloorIndex = 1; FloorIndex < EffectiveFloorCount; ++FloorIndex)
				{
					const double FloorBottom = FloorIndex * EffectiveFloorHeight;
					AddPlacement(EdgeTransform, Length * 0.5, Length * 0.94, 12.0, 6.5, TEXT("insulation_band"), FloorBottom - 6.0, InsulationMaterial);
				}
			}

			if (BlobHasAny(TokenBlob, { TEXT("gruenderzeit"), TEXT("jugendstil"), TEXT("fachwerk"), TEXT("historic"), TEXT("altbau"), TEXT("kontorhaus"), TEXT("church"), TEXT("cathedral"), TEXT("sacral") }))
			{
				UMaterialInterface* OrnamentMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("facade_ornament"), TEXT("Grammar Facade Ornament Bands"), FLinearColor(0.78, 0.72, 0.62, 1.0));
				UMaterialInterface* PilasterMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("facade_ornament"), TEXT("Grammar Facade Ornament Pilasters"), FLinearColor(0.72, 0.66, 0.56, 1.0));

				for (int32 FloorIndex = 1; FloorIndex < EffectiveFloorCount; ++FloorIndex)
				{
					const double FloorBottom = FloorIndex * EffectiveFloorHeight;
					AddPlacement(EdgeTransform, Length * 0.5, Length * 0.92, 16.0, 9.0, TEXT("facade_ornament"), FloorBottom - 8.0, OrnamentMaterial);
				}

				const bool bSacral = BlobHasAny(TokenBlob, { TEXT("church"), TEXT("cathedral"), TEXT("sacral") });
				const int32 PilasterCount = bSacral ? FMath::Clamp(FMath::FloorToInt(Length / 320.0), 2, 8) : FMath::Clamp(FMath::FloorToInt(Length / 400.0), 1, 5);
				for (int32 Index = 1; Index <= PilasterCount; ++Index)
				{
					const double Offset = Length * Index / static_cast<double>(PilasterCount + 1);
					AddPlacement(EdgeTransform, Offset, 16.0, TotalHeight, 8.0, TEXT("facade_ornament"), 0.0, PilasterMaterial);
				}
			}

			// ---- Street-level retail/industrial detail (FacadeDepthPlacements) -- street-facing
			// edge only ----
			if (bStreetFacing)
			{
				const double GroundHeight = EffectiveFloorHeight;

				if (bIsRetail)
				{
					const double SignHeight = FMath::Clamp(GroundHeight * 0.16, 32.0, 62.0);
					const double SignBottom = FMath::Min(FMath::Max(GroundHeight - SignHeight - 35.0, 240.0), FMath::Max(20.0, GroundHeight - SignHeight));
					UMaterialInterface* SignMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("signboard"), TEXT("Grammar Retail Signboards"), FLinearColor(0.88, 0.72, 0.24, 1.0));
					AddPlacement(EdgeTransform, Length * 0.5, Length * 0.82, SignHeight, 8.0, TEXT("signboard"), SignBottom, SignMaterial);

					UMaterialInterface* AwningMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("awning"), TEXT("Grammar Fabric Awnings"), FLinearColor(0.56, 0.08, 0.07, 1.0));
					AddPlacement(EdgeTransform, Length * 0.5, Length * 0.7, 12.0, 85.0, TEXT("awning"), FMath::Max(210.0, SignBottom - 22.0), AwningMaterial);
				}

				if (bIsIndustrial || bIsParking)
				{
					const double DoorWidth = FMath::Min(Length * 0.55, bIsIndustrial ? 480.0 : 560.0);
					const double DoorHeight = FMath::Clamp(GroundHeight - 25.0, 220.0, 420.0);
					UMaterialInterface* GarageMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("garage_door"), TEXT("Grammar Sectional Garage Doors"), FLinearColor(0.24, 0.25, 0.24, 1.0));
					AddPlacement(EdgeTransform, Length * 0.5, DoorWidth, DoorHeight, 9.0, TEXT("garage_door"), 0.0, GarageMaterial);

					if (bIsIndustrial)
					{
						UMaterialInterface* DockMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("loading_dock"), TEXT("Grammar Concrete Loading Docks"), FLinearColor(0.42, 0.42, 0.39, 1.0));
						AddPlacement(EdgeTransform, Length * 0.5, DoorWidth + 100.0, 45.0, 76.0, TEXT("loading_dock"), 0.0, DockMaterial);
					}
				}
			}
			// ---- Stair core (ShouldAddStairCore) -- explicitly NOT the street-facing edge ----
			else if (Length >= 400.0 && TotalHeight >= 700.0 && bStairCoreTokens)
			{
				const double CoreWidth = FMath::Clamp(Length * 0.18, 110.0, 240.0);
				const double CoreHeight = FMath::Max(TotalHeight - 40.0, 100.0);
				UMaterialInterface* StairCoreMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("stair_core"), TEXT("Grammar Stair Core Glass"), FLinearColor(0.09, 0.18, 0.22, 0.82));
				AddPlacement(EdgeTransform, Length * 0.5, CoreWidth, CoreHeight, 6.0, TEXT("stair_core"), 20.0, StairCoreMaterial);
			}
		}

		FPCGTaggedData& PlacementsOut = Context->OutputData.TaggedData.Emplace_GetRef();
		PlacementsOut.Data = PlacementData;
		PlacementsOut.Pin = PlacementsPinLabel;
		PlacementsOut.Tags = Input.Tags;
	}

	return true;
}
