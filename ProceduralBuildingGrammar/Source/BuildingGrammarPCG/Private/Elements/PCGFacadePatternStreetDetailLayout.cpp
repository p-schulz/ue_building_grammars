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
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const FName EdgesPinLabel = TEXT("Edges");
	const FName BuildingInfoPinLabel = TEXT("BuildingInfo");
	const FName StyleInfoPinLabel = TEXT("StyleInfo");
	const FName StreetsPinLabel = TEXT("Streets");
	const FName PlacementsPinLabel = TEXT("Placements");

	// One line segment of a street way, already in world-space UE centimeters (see
	// UPCGGetStreetNetworkSettings' header comment for its own coordinate convention -- matches this
	// pipeline's as long as the same OSM file/origin feeds both nodes). Name is the street's `name`
	// tag if it had one (empty otherwise), carried through so addr:street matching can prefer it the
	// same way FGrammarStreetAlignment::ApplyRidgeDirectionTags does (BuildingGrammarCore/Osm/
	// StreetRidgeAlignment.cpp) -- not reused directly (that class only ever produces a ridge
	// direction, not an edge index, and its nearest-segment helper is private to that .cpp), but the
	// same priority structure (name match beats raw proximity; raw proximity only within a search
	// radius) is deliberately mirrored here.
	struct FStreetSegment
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FString Name;
	};

	// UPCGGetStreetNetworkSettings tags each Streets entry "Name:<value>" (empty for unnamed ways) --
	// same prefix-tag convention as PCGBuildingGrammarDefaults.h's ExtractSourceNameFromTags, just a
	// different prefix, so reimplemented locally rather than generalizing that shared helper for one
	// more caller.
	FString ExtractStreetNameFromTags(const TSet<FString>& Tags)
	{
		static const FString Prefix = TEXT("Name:");
		for (const FString& Tag : Tags)
		{
			if (Tag.StartsWith(Prefix))
			{
				return Tag.Mid(Prefix.Len());
			}
		}
		return FString();
	}

	TMap<FString, FString> DeserializeTagsFromJson(const FString& Json)
	{
		TMap<FString, FString> Tags;
		if (Json.IsEmpty())
		{
			return Tags;
		}
		TSharedPtr<FJsonObject> JsonObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : JsonObject->Values)
			{
				Tags.Add(Pair.Key, Pair.Value->AsString());
			}
		}
		return Tags;
	}

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

	// Every footprint edge's midpoint, in world-space UE centimeters -- the point actually compared
	// against street geometry below (an edge's own nearest-street distance is a more direct measure
	// of "does this edge face a street" than going through the building's centroid the way
	// FGrammarStreetAlignment does for its own, different, ridge-direction purpose).
	FVector EdgeMidpoint(const FPCGPoint& EdgePoint, const FPCGMetadataAttribute<double>* LengthAttr)
	{
		const FVector Start = EdgePoint.Transform.GetLocation();
		const FVector Tangent = EdgePoint.Transform.GetRotation().GetAxisX();
		const double Length = LengthAttr->GetValueFromItemKey(EdgePoint.MetadataEntry);
		return Start + Tangent * (Length * 0.5);
	}

	// Nearest EdgePoints index to the nearest of Candidates, or INDEX_NONE if Candidates is empty.
	int32 NearestEdgeToSegments(const TArray<FPCGPoint>& EdgePoints, const FPCGMetadataAttribute<double>* LengthAttr, const TArray<const FStreetSegment*>& Candidates, double& OutDistance)
	{
		int32 BestIndex = INDEX_NONE;
		double BestDistSq = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < EdgePoints.Num(); ++Index)
		{
			const FVector Mid = EdgeMidpoint(EdgePoints[Index], LengthAttr);
			for (const FStreetSegment* Segment : Candidates)
			{
				const double DistSq = FMath::PointDistToSegmentSquared(Mid, Segment->Start, Segment->End);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestIndex = Index;
				}
			}
		}
		OutDistance = FMath::Sqrt(BestDistSq);
		return BestIndex;
	}

	// Port of GrammarEngineInternal::StreetFacingSideIndex's explicit-tag tiers (see this node's own
	// header comment for why those are a faithful port of a stub, not real street-adjacency
	// computation on their own), extended with two more tiers that DO use real street geometry when
	// the optional "Streets" pin is connected -- addr:street name match (regardless of
	// SearchRadius, an explicit address being a stronger signal than raw proximity -- same reasoning
	// FGrammarStreetAlignment::ApplyRidgeDirectionTags uses), else nearest edge to the nearest street
	// among all of them, only within SearchRadius. Priority, highest first: explicit
	// grammar:street:point/grammar:street_facing_side tag (an explicit per-building override) ->
	// addr:street name match against real street geometry -> nearest real street within SearchRadius
	// -> edge 0. Tag coordinates are in BuildingGrammarCore's meters working unit (the space the
	// footprint was in before this pipeline's meters->cm conversion), converted to cm here to compare
	// against the Edges pin's already-cm Transform locations.
	int32 DetermineStreetFacingSideIndex(const TMap<FString, FString>& Tags, const TArray<FPCGPoint>& EdgePoints, const FPCGMetadataAttribute<double>* LengthAttr, const TArray<FStreetSegment>& StreetSegments, double SearchRadius)
	{
		FString PointValue;
		if (const FString* P1 = Tags.Find(TEXT("grammar:street:point")))
		{
			PointValue = *P1;
		}
		else if (const FString* P2 = Tags.Find(TEXT("grammar:street_point")))
		{
			PointValue = *P2;
		}

		if (!PointValue.IsEmpty())
		{
			TArray<FString> Parts;
			PointValue.Replace(TEXT(";"), TEXT(",")).ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() >= 2)
			{
				const FString XStr = Parts[0].TrimStartAndEnd();
				const FString YStr = Parts[1].TrimStartAndEnd();
				if (FCString::IsNumeric(*XStr) && FCString::IsNumeric(*YStr) && EdgePoints.Num() > 0 && LengthAttr)
				{
					const FVector Point(FCString::Atod(*XStr) * 100.0, FCString::Atod(*YStr) * 100.0, 0.0);
					int32 BestIndex = 0;
					double BestDistSq = TNumericLimits<double>::Max();
					for (int32 Index = 0; Index < EdgePoints.Num(); ++Index)
					{
						const FPCGPoint& EdgePoint = EdgePoints[Index];
						const FVector Start = EdgePoint.Transform.GetLocation();
						const FVector Tangent = EdgePoint.Transform.GetRotation().GetAxisX();
						const double Length = LengthAttr->GetValueFromItemKey(EdgePoint.MetadataEntry);
						const FVector End = Start + Tangent * Length;
						const double DistSq = FMath::PointDistToSegmentSquared(Point, Start, End);
						if (DistSq < BestDistSq)
						{
							BestDistSq = DistSq;
							BestIndex = Index;
						}
					}
					return BestIndex;
				}
			}
		}

		if (const FString* SideValue = Tags.Find(TEXT("grammar:street_facing_side")))
		{
			const FString Trimmed = SideValue->TrimStartAndEnd();
			if (FCString::IsNumeric(*Trimmed) && EdgePoints.Num() > 0)
			{
				const int32 Value = FCString::Atoi(*Trimmed);
				const int32 NumEdges = EdgePoints.Num();
				return ((Value % NumEdges) + NumEdges) % NumEdges;
			}
			// Present but unusable (non-numeric, or no edges) -- fall through to real street
			// geometry below rather than defaulting straight to edge 0.
		}

		if (StreetSegments.Num() > 0 && EdgePoints.Num() > 0 && LengthAttr)
		{
			// addr:street name match, regardless of SearchRadius -- see this function's own comment.
			if (const FString* AddrStreet = Tags.Find(TEXT("addr:street")))
			{
				if (!AddrStreet->IsEmpty())
				{
					const FString Normalized = AddrStreet->TrimStartAndEnd().ToLower();
					TArray<const FStreetSegment*> Matching;
					for (const FStreetSegment& Segment : StreetSegments)
					{
						if (!Segment.Name.IsEmpty() && Segment.Name.TrimStartAndEnd().ToLower() == Normalized)
						{
							Matching.Add(&Segment);
						}
					}
					if (Matching.Num() > 0)
					{
						double MatchDistance = 0.0;
						const int32 MatchIndex = NearestEdgeToSegments(EdgePoints, LengthAttr, Matching, MatchDistance);
						if (MatchIndex != INDEX_NONE)
						{
							return MatchIndex;
						}
					}
				}
			}

			// Nearest edge to the nearest street among ALL of them, only if within SearchRadius.
			TArray<const FStreetSegment*> AllSegments;
			AllSegments.Reserve(StreetSegments.Num());
			for (const FStreetSegment& Segment : StreetSegments)
			{
				AllSegments.Add(&Segment);
			}
			double NearestDistance = 0.0;
			const int32 NearestIndex = NearestEdgeToSegments(EdgePoints, LengthAttr, AllSegments, NearestDistance);
			if (NearestIndex != INDEX_NONE && NearestDistance <= SearchRadius)
			{
				return NearestIndex;
			}
		}

		return 0;
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
