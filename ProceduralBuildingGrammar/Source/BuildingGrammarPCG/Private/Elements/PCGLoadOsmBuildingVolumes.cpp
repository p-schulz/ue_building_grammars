#include "Elements/PCGLoadOsmBuildingVolumes.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGSplineData.h"
#include "Metadata/PCGMetadata.h"
#include "Components/SplineComponent.h"
#include "BuildingGenerationLibrary.h"
#include "Osm/BuildingPartResolver.h"
#include "Grammar/GrammarLevels.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "PCGBuildingGrammarDefaults.h"

UPCGLoadOsmBuildingVolumesSettings::UPCGLoadOsmBuildingVolumesSettings()
{
	LoadDefaultGermanBuildingGrammarConfig(Config);
}

namespace
{
	const FName FootprintsPinLabel = TEXT("Footprints");
	const FName BuildingInfoPinLabel = TEXT("BuildingInfo");

	// Every footprint/placement coordinate downstream of FLocalTangentPlaneProjection is meters in
	// its X=local-North/Y=local-East convention, mapped directly onto UE's world X/Y (see that
	// class's header comment) -- the same MetersToUnrealUnits boundary FGrammarDynamicMeshBuilder
	// and ABuildingInstancePoolActor::ApplyBuildingSpec already cross at exactly this point.
	constexpr double MetersToUnrealUnits = 100.0;

	FString SerializeTagsToJson(const TMap<FString, FString>& Tags)
	{
		const TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& Tag : Tags)
		{
			JsonObject->SetStringField(Tag.Key, Tag.Value);
		}

		FString Result;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
		FJsonSerializer::Serialize(JsonObject, Writer);
		return Result;
	}

	UPCGSplineData* MakeFootprintSplineData(FPCGContext* Context, const FGrammarBuildingVolume& Volume)
	{
		const TArray<FVector2D>& Ring = Volume.Footprint.OuterRing;
		if (Ring.Num() < 3)
		{
			return nullptr;
		}

		TArray<FSplinePoint> SplinePoints;
		SplinePoints.Reserve(Ring.Num());
		for (int32 Index = 0; Index < Ring.Num(); ++Index)
		{
			const FVector Position(Ring[Index].X * MetersToUnrealUnits, Ring[Index].Y * MetersToUnrealUnits, 0.0);
			SplinePoints.Add(FSplinePoint(static_cast<float>(Index), Position, ESplinePointType::Linear));
		}

		UPCGSplineData* SplineData = FPCGContext::NewObject_AnyThread<UPCGSplineData>(Context);
		SplineData->Initialize(SplinePoints, /*bInClosedLoop=*/true, FTransform::Identity);
		return SplineData;
	}
}

TArray<FPCGPinProperties> UPCGLoadOsmBuildingVolumesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(FootprintsPinLabel, EPCGDataType::Spline);
	Pins.Emplace(BuildingInfoPinLabel, EPCGDataType::Param);
	return Pins;
}

FPCGElementPtr UPCGLoadOsmBuildingVolumesSettings::CreateElement() const
{
	return MakeShared<FPCGLoadOsmBuildingVolumesElement>();
}

bool FPCGLoadOsmBuildingVolumesElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPCGLoadOsmBuildingVolumesSettings* Settings = Context->GetInputSettings<UPCGLoadOsmBuildingVolumesSettings>();
	check(Settings);

	if (Settings->OsmFilePath.FilePath.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGLoadOsmBuildingVolumes", "NoFile", "No OSM file path set."));
		return true;
	}

	TArray<FGrammarBuildingVolume> Volumes;
	FString LoadError;
	if (!UBuildingGenerationLibrary::LoadResolvedVolumesFromOsmFile(
		Settings->OsmFilePath.FilePath, Settings->OriginLatitude, Settings->OriginLongitude, Settings->Config, Volumes, LoadError))
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(NSLOCTEXT("PCGLoadOsmBuildingVolumes", "LoadFailed", "Failed to load '{0}': {1}"),
			FText::FromString(Settings->OsmFilePath.FilePath), FText::FromString(LoadError)));
		return true;
	}

	UPCGParamData* BuildingInfo = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
	UPCGMetadata* InfoMetadata = BuildingInfo->MutableMetadata();
	FPCGMetadataAttribute<FString>* SourceNameAttr = InfoMetadata->CreateAttribute<FString>(TEXT("SourceName"), FString(), false, false);
	FPCGMetadataAttribute<double>* MinHeightAttr = InfoMetadata->CreateAttribute<double>(TEXT("MinHeight"), 0.0, false, false);
	FPCGMetadataAttribute<bool>* IsBuildingPartAttr = InfoMetadata->CreateAttribute<bool>(TEXT("IsBuildingPart"), false, false, false);
	FPCGMetadataAttribute<FString>* ParentSourceNameAttr = InfoMetadata->CreateAttribute<FString>(TEXT("ParentSourceName"), FString(), false, false);
	FPCGMetadataAttribute<FString>* BuildingAttr = InfoMetadata->CreateAttribute<FString>(TEXT("Building"), FString(), false, false);
	FPCGMetadataAttribute<FString>* AddrStreetAttr = InfoMetadata->CreateAttribute<FString>(TEXT("AddrStreet"), FString(), false, false);
	FPCGMetadataAttribute<FString>* TagsJsonAttr = InfoMetadata->CreateAttribute<FString>(TEXT("TagsJson"), FString(), false, false);
	FPCGMetadataAttribute<int32>* LevelsAttr = InfoMetadata->CreateAttribute<int32>(TEXT("Levels"), 1, false, false);
	FPCGMetadataAttribute<double>* TotalHeightAttr = InfoMetadata->CreateAttribute<double>(TEXT("TotalHeight"), 0.0, false, false);

	for (const FGrammarBuildingVolume& Volume : Volumes)
	{
		UPCGSplineData* SplineData = MakeFootprintSplineData(Context, Volume);
		if (!SplineData)
		{
			continue;
		}

		FPCGTaggedData& FootprintData = Context->OutputData.TaggedData.Emplace_GetRef();
		FootprintData.Data = SplineData;
		FootprintData.Pin = FootprintsPinLabel;
		FootprintData.Tags.Add(FString::Printf(TEXT("SourceName:%s"), *Volume.SourceName));

		const int64 EntryKey = BuildingInfo->FindOrAddMetadataKey(FName(*Volume.SourceName));
		SourceNameAttr->SetValue(EntryKey, Volume.SourceName);
		MinHeightAttr->SetValue(EntryKey, Volume.MinHeight);
		IsBuildingPartAttr->SetValue(EntryKey, Volume.bIsBuildingPart);
		ParentSourceNameAttr->SetValue(EntryKey, Volume.ParentSourceName);
		if (const FString* BuildingValue = Volume.VolumeTags.Find(TEXT("building")))
		{
			BuildingAttr->SetValue(EntryKey, *BuildingValue);
		}
		if (const FString* AddrStreetValue = Volume.VolumeTags.Find(TEXT("addr:street")))
		{
			AddrStreetAttr->SetValue(EntryKey, *AddrStreetValue);
		}
		TagsJsonAttr->SetValue(EntryKey, SerializeTagsToJson(Volume.VolumeTags));

		// Reuses the classic engine's own FGrammarLevels rather than reimplementing OSM
		// building:levels/levels/height parsing -- see this class's header comment.
		const int32 Levels = FGrammarLevels::InferLevels(Volume.VolumeTags, Settings->Config, nullptr);
		const TArray<double> FloorHeightsMeters = FGrammarLevels::FloorHeightSequence(Levels, Volume.VolumeTags, Settings->Config, nullptr);
		double TotalHeightMeters = 0.0;
		for (const double FloorHeightMeters : FloorHeightsMeters)
		{
			TotalHeightMeters += FloorHeightMeters;
		}
		LevelsAttr->SetValue(EntryKey, Levels);
		TotalHeightAttr->SetValue(EntryKey, TotalHeightMeters * MetersToUnrealUnits);
	}

	FPCGTaggedData& BuildingInfoData = Context->OutputData.TaggedData.Emplace_GetRef();
	BuildingInfoData.Data = BuildingInfo;
	BuildingInfoData.Pin = BuildingInfoPinLabel;

	return true;
}
