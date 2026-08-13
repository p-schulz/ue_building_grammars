#include "Elements/PCGRoofServiceAntennaLayout.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Geometry/GrammarRoofFrame.h"
#include "Config/AntennaStyleConfig.h"
#include "GrammarKitResolver.h"
#include "PCGBuildingGrammarDefaults.h"
#include "Math/RotationMatrix.h"
#include "Algo/Reverse.h"
#include "UObject/SoftObjectPath.h"
#include "Materials/MaterialInterface.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const FName FootprintPinLabel = TEXT("Footprint");
	const FName BuildingInfoPinLabel = TEXT("BuildingInfo");
	const FName StyleInfoPinLabel = TEXT("StyleInfo");
	const FName PlacementsPinLabel = TEXT("Placements");

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

	// Narrow, self-contained approximation of GrammarEngineInternal::StyleTokens+HasAny -- see this
	// node's own header comment for why (private/non-exported function, and this only needs a
	// substring blob, not exact tokenization).
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

	FQuat RotationFromTangentNormal(const FVector2D& Tangent, const FVector2D& Normal)
	{
		return FRotationMatrix::MakeFromXY(FVector(Tangent.X, Tangent.Y, 0.0), FVector(Normal.X, Normal.Y, 0.0)).ToQuat();
	}

	// Reverse counterpart of PCGSelectFacadeStyle.cpp's RoofTypeToString -- spelling only needs to
	// match between the two, not any UI-facing text.
	EGrammarRoofType RoofTypeFromString(const FString& Value)
	{
		if (Value == TEXT("Gabled")) return EGrammarRoofType::Gabled;
		if (Value == TEXT("Hipped")) return EGrammarRoofType::Hipped;
		if (Value == TEXT("Pyramid")) return EGrammarRoofType::Pyramid;
		return EGrammarRoofType::Flat;
	}

	// EGrammarAntennaType round-trip counterpart to PCGSelectFacadeStyle.cpp's AntennaTypeToString --
	// spelling only needs to match between these two, not any UI-facing text.
	EGrammarAntennaType AntennaTypeFromString(const FString& Value)
	{
		if (Value == TEXT("Radio")) return EGrammarAntennaType::Radio;
		if (Value == TEXT("Satellite")) return EGrammarAntennaType::Satellite;
		if (Value == TEXT("LightningRod")) return EGrammarAntennaType::LightningRod;
		if (Value == TEXT("Cellular")) return EGrammarAntennaType::Cellular;
		if (Value == TEXT("OfficeCluster")) return EGrammarAntennaType::OfficeCluster;
		if (Value == TEXT("Broadcast")) return EGrammarAntennaType::Broadcast;
		if (Value == TEXT("LampPost")) return EGrammarAntennaType::LampPost;
		return EGrammarAntennaType::Tv;
	}
}

TArray<FPCGPinProperties> UPCGRoofServiceAntennaLayoutSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(FootprintPinLabel, EPCGDataType::Spline);
	Pins.Emplace(BuildingInfoPinLabel, EPCGDataType::Param);
	// Not required -- without it, antennas are skipped entirely (see this class's header comment) and
	// PV/HVAC/plant token gating just won't see any StyleName contribution.
	Pins.Emplace(StyleInfoPinLabel, EPCGDataType::Param);
	return Pins;
}

TArray<FPCGPinProperties> UPCGRoofServiceAntennaLayoutSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(PlacementsPinLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGRoofServiceAntennaLayoutSettings::CreateElement() const
{
	return MakeShared<FPCGRoofServiceAntennaLayoutElement>();
}

bool FPCGRoofServiceAntennaLayoutElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGRoofServiceAntennaLayoutSettings* Settings = Context->GetInputSettings<UPCGRoofServiceAntennaLayoutSettings>();
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
	const FPCGMetadataAttribute<FString>* RoofTypeAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("RoofType")) : nullptr;
	const FPCGMetadataAttribute<bool>* AntennaEnabledAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<bool>(TEXT("AntennaEnabled")) : nullptr;
	const FPCGMetadataAttribute<FString>* AntennaTypeAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("AntennaType")) : nullptr;
	const FPCGMetadataAttribute<int32>* AntennaCountAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<int32>(TEXT("AntennaCount")) : nullptr;
	const FPCGMetadataAttribute<double>* AntennaMastHeightAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("AntennaMastHeight")) : nullptr;
	const FPCGMetadataAttribute<double>* AntennaMastRadiusAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("AntennaMastRadius")) : nullptr;
	const FPCGMetadataAttribute<double>* AntennaBaseWidthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("AntennaBaseWidth")) : nullptr;
	const FPCGMetadataAttribute<double>* AntennaBaseDepthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("AntennaBaseDepth")) : nullptr;
	const FPCGMetadataAttribute<double>* AntennaBaseHeightAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("AntennaBaseHeight")) : nullptr;
	const FPCGMetadataAttribute<double>* AntennaPanelWidthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("AntennaPanelWidth")) : nullptr;
	const FPCGMetadataAttribute<double>* AntennaPanelHeightAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("AntennaPanelHeight")) : nullptr;
	const FPCGMetadataAttribute<double>* AntennaPanelDepthAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<double>(TEXT("AntennaPanelDepth")) : nullptr;
	const FPCGMetadataAttribute<FString>* AntennaMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("AntennaMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* AntennaColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("AntennaColor")) : nullptr;
	const FPCGMetadataAttribute<FString>* AntennaAccentMaterialAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FString>(TEXT("AntennaAccentMaterial")) : nullptr;
	const FPCGMetadataAttribute<FVector4>* AntennaAccentColorAttr = StyleMetadata ? StyleMetadata->GetConstTypedAttribute<FVector4>(TEXT("AntennaAccentColor")) : nullptr;

	// BuildingInfo correlation (Tags, for token gating) -- separate lookup from StyleInfo, keyed by
	// the same "SourceName:..." tag every node in this pipeline propagates.
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
	const FPCGMetadataAttribute<double>* TotalHeightAttr = InfoMetadata ? InfoMetadata->GetConstTypedAttribute<double>(TEXT("TotalHeight")) : nullptr;

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(FootprintPinLabel);
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSplineData* SplineData = Cast<UPCGSplineData>(Input.Data.Get());
		if (!SplineData)
		{
			continue;
		}

		TArray<FSplinePoint> SplinePoints = SplineData->GetSplinePoints();
		const int32 Count = SplinePoints.Num();
		if (Count < 3)
		{
			continue;
		}

		double SignedArea = 0.0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector& A = SplinePoints[Index].Position;
			const FVector& B = SplinePoints[(Index + 1) % Count].Position;
			SignedArea += (A.X * B.Y - B.X * A.Y);
		}
		if (SignedArea < 0.0)
		{
			Algo::Reverse(SplinePoints);
		}

		TArray<FVector2D> Footprint2D;
		Footprint2D.Reserve(Count);
		FVector2D Centroid = FVector2D::ZeroVector;
		FVector2D BoundsMin(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
		FVector2D BoundsMax(-TNumericLimits<double>::Max(), -TNumericLimits<double>::Max());
		for (const FSplinePoint& Point : SplinePoints)
		{
			const FVector2D P(Point.Position.X, Point.Position.Y);
			Footprint2D.Add(P);
			Centroid += P;
			BoundsMin = FVector2D(FMath::Min(BoundsMin.X, P.X), FMath::Min(BoundsMin.Y, P.Y));
			BoundsMax = FVector2D(FMath::Max(BoundsMax.X, P.X), FMath::Max(BoundsMax.Y, P.Y));
		}
		Centroid /= static_cast<double>(Count);

		const FString SourceName = ExtractSourceNameFromTags(Input.Tags);
		FString StyleName;
		TMap<FString, FString> Tags;
		const int64 InfoEntryKey = BuildingInfo ? BuildingInfo->FindMetadataKey(FName(*SourceName)) : INDEX_NONE;
		if (InfoEntryKey != INDEX_NONE && TagsJsonAttr)
		{
			Tags = DeserializeTagsFromJson(TagsJsonAttr->GetValueFromItemKey(InfoEntryKey));
		}
		const int64 StyleEntryKey = StyleInfo ? StyleInfo->FindMetadataKey(FName(*SourceName)) : INDEX_NONE;
		if (StyleEntryKey != INDEX_NONE && StyleNameAttr)
		{
			StyleName = StyleNameAttr->GetValueFromItemKey(StyleEntryKey);
		}
		const FString TokenBlob = BuildTokenBlob(StyleName, Tags);

		// Resolved per building -- see UPCGRoofFrameGeneratorSettings' identical resolution for why
		// (this building's own StyleInfo row, if present, supplies its actual roof Type, gating both
		// the antenna RoofZ bump below and whether PV/HVAC/plant apply at all).
		EGrammarRoofType EffectiveRoofType = Settings->RoofType;
		if (StyleEntryKey != INDEX_NONE && RoofTypeAttr)
		{
			EffectiveRoofType = RoofTypeFromString(RoofTypeAttr->GetValueFromItemKey(StyleEntryKey));
		}
		const bool bFlat = (EffectiveRoofType == EGrammarRoofType::Flat);

		// This building's own OSM-derived TotalHeight (see UPCGLoadOsmBuildingVolumesSettings'
		// header comment) if BuildingInfo has a usable row, else this node's own flat EaveHeight --
		// see this class's header comment.
		double EffectiveEaveHeight = Settings->EaveHeight;
		if (InfoEntryKey != INDEX_NONE && TotalHeightAttr)
		{
			const double TotalHeight = TotalHeightAttr->GetValueFromItemKey(InfoEntryKey);
			if (TotalHeight > 0.0)
			{
				EffectiveEaveHeight = TotalHeight;
			}
		}

		UPCGPointData* PlacementData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		UPCGMetadata* PlacementMetadata = PlacementData->MutableMetadata();
		FPCGMetadataAttribute<FString>* RoleAttr = PlacementMetadata->CreateAttribute<FString>(TEXT("Role"), FString(), false, false);
		FPCGMetadataAttribute<double>* WidthAttr = PlacementMetadata->CreateAttribute<double>(TEXT("Width"), 0.0, false, false);
		FPCGMetadataAttribute<double>* HeightAttr = PlacementMetadata->CreateAttribute<double>(TEXT("Height"), 0.0, false, false);
		FPCGMetadataAttribute<FSoftObjectPath>* MaterialOverrideAttr = PlacementMetadata->CreateAttribute<FSoftObjectPath>(TEXT("MaterialOverride"), FSoftObjectPath(), false, false);

		auto MakePlacementPoint = [&](const FQuat& Rotation, const FVector& WorldCenter, double Width, double Height, double Depth, const TCHAR* Role, UMaterialInterface* Material)
		{
			FPCGPoint Point;
			Point.Transform = FTransform(Rotation, WorldCenter, FVector(Width, Depth, Height));
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

		// ---- Antennas (AntennaPlacements) ----
		if (StyleEntryKey != INDEX_NONE && AntennaEnabledAttr && AntennaEnabledAttr->GetValueFromItemKey(StyleEntryKey))
		{
			const int32 AntennaCount = AntennaCountAttr ? AntennaCountAttr->GetValueFromItemKey(StyleEntryKey) : 0;
			const double MastHeight = AntennaMastHeightAttr ? AntennaMastHeightAttr->GetValueFromItemKey(StyleEntryKey) : 0.0;
			if (AntennaCount > 0 && MastHeight > 0.0)
			{
				double RoofZ = EffectiveEaveHeight;
				if (bFlat && Settings->bEdgeEnabled)
				{
					RoofZ += FMath::Max(Settings->EdgeHeight - Settings->SurfaceInset, 0.0);
				}

				const double MastRadius = AntennaMastRadiusAttr ? AntennaMastRadiusAttr->GetValueFromItemKey(StyleEntryKey) : 0.0;
				const double BaseWidth = AntennaBaseWidthAttr ? AntennaBaseWidthAttr->GetValueFromItemKey(StyleEntryKey) : 0.0;
				const double BaseDepth = AntennaBaseDepthAttr ? AntennaBaseDepthAttr->GetValueFromItemKey(StyleEntryKey) : 0.0;
				const double BaseHeight = AntennaBaseHeightAttr ? AntennaBaseHeightAttr->GetValueFromItemKey(StyleEntryKey) : 0.0;
				const double PanelWidth = AntennaPanelWidthAttr ? AntennaPanelWidthAttr->GetValueFromItemKey(StyleEntryKey) : 0.0;
				const double PanelHeight = AntennaPanelHeightAttr ? AntennaPanelHeightAttr->GetValueFromItemKey(StyleEntryKey) : 0.0;
				const double PanelDepth = AntennaPanelDepthAttr ? AntennaPanelDepthAttr->GetValueFromItemKey(StyleEntryKey) : 0.0;
				const EGrammarAntennaType Type = AntennaTypeAttr ? AntennaTypeFromString(AntennaTypeAttr->GetValueFromItemKey(StyleEntryKey)) : EGrammarAntennaType::Tv;

				UMaterialInterface* MastMaterial = nullptr;
				UMaterialInterface* AccentMaterial = nullptr;
				if (AntennaMaterialAttr && AntennaColorAttr)
				{
					const FVector4 ColorValue = AntennaColorAttr->GetValueFromItemKey(StyleEntryKey);
					MastMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("antenna"), AntennaMaterialAttr->GetValueFromItemKey(StyleEntryKey), FLinearColor(ColorValue.X, ColorValue.Y, ColorValue.Z, ColorValue.W));
				}
				if (AntennaAccentMaterialAttr && AntennaAccentColorAttr)
				{
					const FVector4 ColorValue = AntennaAccentColorAttr->GetValueFromItemKey(StyleEntryKey);
					AccentMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("antenna_panel"), AntennaAccentMaterialAttr->GetValueFromItemKey(StyleEntryKey), FLinearColor(ColorValue.X, ColorValue.Y, ColorValue.Z, ColorValue.W));
				}

				// Instance positions: Centroid, then up to 4 more inset-corner candidates -- port of
				// AntennaPositions, truncated to min(Count,5).
				TArray<FVector2D> Positions;
				if (AntennaCount <= 1)
				{
					Positions.Add(Centroid);
				}
				else
				{
					const double InsetX = FMath::Max((BoundsMax.X - BoundsMin.X) * 0.22, 50.0);
					const double InsetY = FMath::Max((BoundsMax.Y - BoundsMin.Y) * 0.22, 50.0);
					TArray<FVector2D> Candidates = {
						Centroid,
						FVector2D(BoundsMax.X - InsetX, BoundsMax.Y - InsetY),
						FVector2D(BoundsMin.X + InsetX, BoundsMax.Y - InsetY),
						FVector2D(BoundsMax.X - InsetX, BoundsMin.Y + InsetY),
						FVector2D(BoundsMin.X + InsetX, BoundsMin.Y + InsetY)
					};
					const int32 UseCount = FMath::Min(AntennaCount, Candidates.Num());
					for (int32 Index = 0; Index < UseCount; ++Index)
					{
						Positions.Add(Candidates[Index]);
					}
				}

				const FVector2D WorldX(1.0, 0.0);
				const FVector2D WorldY(0.0, 1.0);
				const FQuat DefaultRotation = RotationFromTangentNormal(WorldX, WorldY);
				const FQuat SwappedRotation = RotationFromTangentNormal(WorldY, WorldX);
				const double MastWidth = FMath::Max(MastRadius * 2.0, 2.5);

				for (const FVector2D& Position : Positions)
				{
					MakePlacementPoint(DefaultRotation, FVector(Position.X, Position.Y, RoofZ + BaseHeight * 0.5), BaseWidth, BaseHeight, BaseDepth, TEXT("antenna"), MastMaterial);
					MakePlacementPoint(DefaultRotation, FVector(Position.X, Position.Y, RoofZ + BaseHeight + MastHeight * 0.5), MastWidth, MastHeight, MastWidth, TEXT("antenna"), MastMaterial);
					const double TopZ = RoofZ + BaseHeight + MastHeight;

					switch (Type)
					{
					case EGrammarAntennaType::Cellular:
					case EGrammarAntennaType::OfficeCluster:
					{
						const int32 PanelCount = (Type == EGrammarAntennaType::OfficeCluster) ? 4 : 3;
						for (int32 PanelIndex = 0; PanelIndex < PanelCount; ++PanelIndex)
						{
							const int32 Direction = PanelIndex % 4;
							const FVector2D PanelTangent = (Direction % 2 == 0) ? WorldX : WorldY;
							FVector2D PanelNormal;
							switch (Direction)
							{
							case 0: PanelNormal = FVector2D(0.0, 1.0); break;
							case 1: PanelNormal = FVector2D(1.0, 0.0); break;
							case 2: PanelNormal = FVector2D(0.0, -1.0); break;
							default: PanelNormal = FVector2D(-1.0, 0.0); break;
							}
							const FVector2D PanelCenter2D = Position + PanelNormal * (PanelDepth + 8.0);
							MakePlacementPoint(RotationFromTangentNormal(PanelTangent, PanelNormal), FVector(PanelCenter2D.X, PanelCenter2D.Y, TopZ - PanelHeight * 0.35), PanelWidth, PanelHeight, PanelDepth, TEXT("antenna_panel"), AccentMaterial);
						}
						break;
					}
					case EGrammarAntennaType::Satellite:
					{
						const FVector2D DishCenter2D = Position + WorldX * (PanelDepth + 8.0);
						MakePlacementPoint(SwappedRotation, FVector(DishCenter2D.X, DishCenter2D.Y, TopZ - PanelHeight * 0.15), PanelWidth, PanelHeight, PanelDepth, TEXT("antenna_panel"), AccentMaterial);
						const FVector2D ArmCenter2D = Position + WorldX * (PanelDepth * 0.75);
						MakePlacementPoint(DefaultRotation, FVector(ArmCenter2D.X, ArmCenter2D.Y, TopZ - PanelHeight * 0.2 + MastWidth * 0.5), PanelDepth * 2.0, MastWidth, MastWidth, TEXT("antenna_panel"), MastMaterial);
						break;
					}
					case EGrammarAntennaType::Broadcast:
					{
						for (int32 BarIndex = 0; BarIndex < 3; ++BarIndex)
						{
							static const double ZFactors[3] = { 0.35, 0.62, 0.88 };
							const FQuat BarRotation = (BarIndex % 2 == 0) ? DefaultRotation : SwappedRotation;
							const double Z = RoofZ + BaseHeight + MastHeight * ZFactors[BarIndex] + MastWidth * 0.5;
							MakePlacementPoint(BarRotation, FVector(Position.X, Position.Y, Z), PanelWidth * 3.0, MastWidth, MastWidth, TEXT("antenna_panel"), AccentMaterial);
						}
						break;
					}
					case EGrammarAntennaType::LightningRod:
					{
						const double TipHeight = FMath::Max(MastHeight * 0.28, 25.0);
						MakePlacementPoint(DefaultRotation, FVector(Position.X, Position.Y, TopZ + TipHeight * 0.5), MastWidth * 0.6, TipHeight, MastWidth * 0.6, TEXT("antenna_panel"), AccentMaterial);
						break;
					}
					case EGrammarAntennaType::LampPost:
					{
						const FVector2D LampCenter2D = Position + WorldX * (PanelDepth * 0.6);
						const double LampHeight = FMath::Max(PanelHeight, MastWidth * 2.0);
						MakePlacementPoint(DefaultRotation, FVector(LampCenter2D.X, LampCenter2D.Y, TopZ), FMath::Max(PanelWidth, MastWidth * 3.0), LampHeight, FMath::Max(PanelDepth, MastWidth * 1.8), TEXT("roof_lamp"), AccentMaterial);
						break;
					}
					default: // Tv, Radio
					{
						const double BarWidth = PanelWidth * ((Type == EGrammarAntennaType::Radio) ? 2.2 : 1.6);
						for (int32 BarIndex = 0; BarIndex < 2; ++BarIndex)
						{
							static const double ZFactors[2] = { 0.55, 0.78 };
							const FQuat BarRotation = (BarIndex % 2 == 0) ? DefaultRotation : SwappedRotation;
							const double Z = RoofZ + BaseHeight + MastHeight * ZFactors[BarIndex] + MastWidth * 0.5;
							MakePlacementPoint(BarRotation, FVector(Position.X, Position.Y, Z), BarWidth, MastWidth, MastWidth, TEXT("antenna_panel"), AccentMaterial);
						}
						break;
					}
					}
				}
			}
		}

		// ---- PV panels / HVAC units / plant screens (RoofServicePlacements) -- Flat roofs only ----
		if (bFlat)
		{
			const TArray<FVector> Base = FGrammarRoofFrameMath::RoofBaseVertices(Footprint2D, EffectiveEaveHeight, FMath::Max(Settings->Overhang, 0.0));
			FVector2D ServiceMin(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
			FVector2D ServiceMax(-TNumericLimits<double>::Max(), -TNumericLimits<double>::Max());
			for (const FVector& Point : Base)
			{
				ServiceMin = FVector2D(FMath::Min(ServiceMin.X, Point.X), FMath::Min(ServiceMin.Y, Point.Y));
				ServiceMax = FVector2D(FMath::Max(ServiceMax.X, Point.X), FMath::Max(ServiceMax.Y, Point.Y));
			}
			const double ServiceWidth = ServiceMax.X - ServiceMin.X;
			const double ServiceDepth = ServiceMax.Y - ServiceMin.Y;

			if (ServiceWidth > 100.0 && ServiceDepth > 100.0)
			{
				const double RoofZ = Settings->bEdgeEnabled ? EffectiveEaveHeight + FMath::Max(Settings->EdgeHeight - Settings->SurfaceInset, 0.0) : EffectiveEaveHeight;
				const FVector2D Axis = (ServiceWidth >= ServiceDepth) ? FVector2D(1.0, 0.0) : FVector2D(0.0, 1.0);
				const FVector2D ServiceNormal = (ServiceWidth >= ServiceDepth) ? FVector2D(0.0, 1.0) : FVector2D(1.0, 0.0);
				const FVector2D ServiceCenter = (ServiceMin + ServiceMax) * 0.5;
				const FQuat ServiceRotation = RotationFromTangentNormal(Axis, ServiceNormal);
				const double LongerSpan = FMath::Max(ServiceWidth, ServiceDepth);

				if (BlobHasAny(TokenBlob, { TEXT("office"), TEXT("industrial"), TEXT("warehouse"), TEXT("retail"), TEXT("supermarket"), TEXT("modern"), TEXT("passivhaus") }))
				{
					UMaterialInterface* PvMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("pv_panel"), TEXT("Grammar Roof PV Panels"), FLinearColor(0.04, 0.07, 0.09, 1.0));
					const int32 PanelCount = (LongerSpan > 1200.0) ? 4 : 2;
					for (int32 Index = 0; Index < PanelCount; ++Index)
					{
						const double Lateral = (Index - (PanelCount - 1) / 2.0) * 145.0;
						const FVector2D PanelCenter2D = ServiceCenter + ServiceNormal * Lateral;
						MakePlacementPoint(ServiceRotation, FVector(PanelCenter2D.X, PanelCenter2D.Y, RoofZ + 9.0), FMath::Min(LongerSpan * 0.32, 380.0), 8.0, 82.0, TEXT("pv_panel"), PvMaterial);
					}
				}

				if (BlobHasAny(TokenBlob, { TEXT("office"), TEXT("industrial"), TEXT("warehouse"), TEXT("retail"), TEXT("supermarket") }))
				{
					UMaterialInterface* HvacMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("hvac_unit"), TEXT("Grammar Roof HVAC Units"), FLinearColor(0.52, 0.54, 0.52, 1.0));
					const int32 HvacCount = (LongerSpan > 1600.0) ? 3 : 1;
					for (int32 Index = 0; Index < HvacCount; ++Index)
					{
						const double Shift = (Index - (HvacCount - 1) / 2.0) * 170.0;
						const FVector2D UnitCenter2D = ServiceCenter + Axis * Shift - FVector2D(ServiceNormal.X * ServiceDepth * 0.16, ServiceNormal.Y * ServiceWidth * 0.16);
						MakePlacementPoint(ServiceRotation, FVector(UnitCenter2D.X, UnitCenter2D.Y, RoofZ + 32.5), 110.0, 55.0, 82.0, TEXT("hvac_unit"), HvacMaterial);
					}
				}

				if (BlobHasAny(TokenBlob, { TEXT("office"), TEXT("industrial"), TEXT("warehouse"), TEXT("supermarket"), TEXT("parking") }))
				{
					UMaterialInterface* PlantMaterial = FGrammarKitResolver::ResolveMaterial(StyleName, TEXT("roof_plant"), TEXT("Grammar Roof Plant Screens"), FLinearColor(0.24, 0.25, 0.24, 1.0));
					const double PlantWidth = FMath::Min(LongerSpan * 0.42, 550.0);
					const FVector2D PlantCenter2D = ServiceCenter + FVector2D(ServiceNormal.X * ServiceDepth * 0.22, ServiceNormal.Y * ServiceWidth * 0.22);
					MakePlacementPoint(ServiceRotation, FVector(PlantCenter2D.X, PlantCenter2D.Y, RoofZ + 57.5), PlantWidth, 105.0, 18.0, TEXT("roof_plant"), PlantMaterial);
				}
			}
		}

		FPCGTaggedData& PlacementsOut = Context->OutputData.TaggedData.Emplace_GetRef();
		PlacementsOut.Data = PlacementData;
		PlacementsOut.Pin = PlacementsPinLabel;
		PlacementsOut.Tags = Input.Tags;
	}

	return true;
}
