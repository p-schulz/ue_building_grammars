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

	// Overridden from the loaded config's own value (which defaults to true, matching classic's own
	// default -- this only changes THIS node's own Config instance, not the shared default any other
	// caller of FBuildingPartResolver gets). With bSkipParentFootprintsWithParts left true, a
	// `building` way's ENTIRE footprint is dropped the moment it has any matched `building:part`
	// child, even if those parts don't fully tile it -- leaving a hole rather than an overlap. PCG
	// instead emits both the parent and its parts and relies on
	// UPCGExtrudeFootprintToWallsSettings::bSuppressOverlappingWalls to reconcile the overlap between
	// them (a building:part's own walls always win over its parent's), which also correctly fills in
	// whatever part of the parent's footprint its parts don't cover -- see that node's own header
	// comment for the full algorithm.
	Config.bSkipParentFootprintsWithParts = false;
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

		// Baking MinHeight into the footprint's own Z (instead of 0.0) is the single source of truth
		// this pipeline uses for FBuildingGrammarEngine::ApplyMinHeightOffset's port (see this node's
		// header comment) -- UPCGExtrudeFootprintToWallsSettings extrudes from Start.Z/End.Z rather
		// than an assumed 0.0, so a building part's walls (and, via the "Edges" pin's Transform
		// location, its windows/doors/facade-pattern detail) automatically start at the right
		// elevation with no further change needed in those nodes. Roof-generating nodes take
		// EaveHeight as an independent absolute Z (they only read the footprint's X/Y, never its Z),
		// so they need their own explicit MinHeight read from this same BuildingInfo output instead.
		const double MinHeightUnrealUnits = Volume.MinHeight * MetersToUnrealUnits;

		TArray<FSplinePoint> SplinePoints;
		SplinePoints.Reserve(Ring.Num());
		for (int32 Index = 0; Index < Ring.Num(); ++Index)
		{
			const FVector Position(Ring[Index].X * MetersToUnrealUnits, Ring[Index].Y * MetersToUnrealUnits, MinHeightUnrealUnits);
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

	// Every SourceName that appears as some OTHER volume's ParentSourceName -- i.e. every volume that
	// HAS at least one building:part child. Computed once up front so HasBuildingPartsAttr (below)
	// can be set correctly regardless of a volume's position in Volumes relative to its children.
	// Consumed by UPCGRoofFrameGeneratorSettings to skip generating a roof for a parent that has
	// parts -- see this node's own header comment (Config.bSkipParentFootprintsWithParts=false) and
	// that node's own header comment for why: walls are reconciled per-edge by
	// UPCGExtrudeFootprintToWallsSettings' overlap suppression, but nothing does the equivalent for
	// roofs yet, so a parent's own roof would otherwise render in full, overlapping every part's own
	// roof wherever they coincide in plan view -- reverting to no-roof-for-the-parent (matching this
	// node's old, still-current-for-classic bSkipParentFootprintsWithParts=true behavior) avoids that
	// regression rather than trading a missing-wall hole for a duplicated-roof overlap.
	TSet<FString> ParentsWithChildren;
	ParentsWithChildren.Reserve(Volumes.Num());
	for (const FGrammarBuildingVolume& Volume : Volumes)
	{
		if (Volume.bIsBuildingPart && !Volume.ParentSourceName.IsEmpty())
		{
			ParentsWithChildren.Add(Volume.ParentSourceName);
		}
	}

	UPCGParamData* BuildingInfo = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
	UPCGMetadata* InfoMetadata = BuildingInfo->MutableMetadata();
	FPCGMetadataAttribute<FString>* SourceNameAttr = InfoMetadata->CreateAttribute<FString>(TEXT("SourceName"), FString(), false, false);
	FPCGMetadataAttribute<double>* MinHeightAttr = InfoMetadata->CreateAttribute<double>(TEXT("MinHeight"), 0.0, false, false);
	FPCGMetadataAttribute<bool>* IsBuildingPartAttr = InfoMetadata->CreateAttribute<bool>(TEXT("IsBuildingPart"), false, false, false);
	FPCGMetadataAttribute<FString>* ParentSourceNameAttr = InfoMetadata->CreateAttribute<FString>(TEXT("ParentSourceName"), FString(), false, false);
	FPCGMetadataAttribute<bool>* HasBuildingPartsAttr = InfoMetadata->CreateAttribute<bool>(TEXT("HasBuildingParts"), false, false, false);
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
		HasBuildingPartsAttr->SetValue(EntryKey, ParentsWithChildren.Contains(Volume.SourceName));
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
