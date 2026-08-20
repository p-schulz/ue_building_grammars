#include "BuildingGrammarEdModeSettings.h"

#include "BuildingGenerationLibrary.h"
#include "BuildingInstancePoolActor.h"
#include "Config/GrammarConfigJson.h"
#include "Footprint/BuildingFootprint.h"
#include "Osm/BuildingPartResolver.h"
#include "Osm/FlexOsmBuildingFootprints.h"
#include "Osm/FlexOsmImportContextActor.h"
#include "Presets/GrammarBuildingPresets.h"
#include "GeoReferenceOriginActor.h"
#include "EngineUtils.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "BuildingGrammarEdModeSettings"

namespace
{
	void SyncBuildingGrammarGeoReference(UWorld* World, const AFlexOsmImportContextActor* Context)
	{
		if (World && Context && Context->bHasResolvedOrigin)
		{
			AGeoReferenceOriginActor::SetInWorld(
				World, Context->ResolvedOriginLatLon.X, Context->ResolvedOriginLatLon.Y);
		}
	}
}

bool UBuildingGrammarEdModeSettings::ResolveConfig(FBuildingGrammarConfig& OutConfig, FString& OutError) const
{
	if (GrammarConfigFile.FilePath.IsEmpty())
	{
		OutConfig = GrammarBuildingPresets::UrbanBlockConfig();
		return true;
	}

	FString ConfigPath = GrammarConfigFile.FilePath;
	if (FPaths::IsRelative(ConfigPath))
	{
		ConfigPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ConfigPath);
	}
	return FGrammarConfigJson::LoadConfigFromPythonJsonFile(ConfigPath, OutConfig, OutError);
}

void UBuildingGrammarEdModeSettings::LoadGrammarConfig()
{
	FBuildingGrammarConfig Config;
	FString Error;
	if (!ResolveConfig(Config, Error))
	{
		LastResult = FString::Printf(TEXT("Grammar config failed: %s"), *Error);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(LastResult));
		return;
	}

	CachedPlacementConfig = Config;
	bCachedPlacementConfigValid = true;
	// The previously-active style may no longer exist in a newly loaded config.
	if (!ActiveStyleName.IsEmpty() && !Config.Styles.ContainsByPredicate(
		[this](const FFacadeStyleConfig& Style) { return Style.Name == ActiveStyleName; }))
	{
		ActiveStyleName.Reset();
	}

	LastResult = GrammarConfigFile.FilePath.IsEmpty()
		? FString::Printf(TEXT("Built-in Urban Block config is valid (%d styles)."), Config.Styles.Num())
		: FString::Printf(TEXT("Loaded %d styles from %s."), Config.Styles.Num(), *FPaths::GetCleanFilename(GrammarConfigFile.FilePath));
}

const FBuildingGrammarConfig& UBuildingGrammarEdModeSettings::GetResolvedConfigForPlacement() const
{
	if (!bCachedPlacementConfigValid)
	{
		FString Error;
		if (!ResolveConfig(CachedPlacementConfig, Error))
		{
			CachedPlacementConfig = GrammarBuildingPresets::UrbanBlockConfig();
		}
		bCachedPlacementConfigValid = true;
	}
	return CachedPlacementConfig;
}

TArray<FString> UBuildingGrammarEdModeSettings::GetStyleNameOptions() const
{
	TArray<FString> Options;
	Options.Add(FString()); // Empty = "Auto" (tag-based selection, today's default behavior).
	for (const FFacadeStyleConfig& Style : GetResolvedConfigForPlacement().Styles)
	{
		Options.Add(Style.Name);
	}
	return Options;
}

void UBuildingGrammarEdModeSettings::LoadFlexNetworkOsmContext()
{
	AFlexOsmImportContextActor* Context = AFlexOsmImportContextActor::Find(TargetWorld.Get());
	if (!Context || !Context->OsmAsset)
	{
		LastResult = TEXT("This level has no FlexNetwork OSM context. Generate roads or publish a context first.");
		return;
	}
	OsmAsset = Context->OsmAsset;
	OsmImportSettings = Context->ImportSettings;
	SyncBuildingGrammarGeoReference(TargetWorld.Get(), Context);
	LastResult = Context->bHasResolvedOrigin
		? FString::Printf(TEXT("Loaded the shared FlexNetwork OSM context at origin %.8f, %.8f."),
			Context->ResolvedOriginLatLon.X, Context->ResolvedOriginLatLon.Y)
		: TEXT("Loaded the shared FlexNetwork OSM context; its origin is unresolved.");
}

void UBuildingGrammarEdModeSettings::PublishFlexNetworkOsmContext()
{
	UWorld* World = TargetWorld.Get();
	if (!World || !OsmAsset)
	{
		LastResult = TEXT("Select an OSM asset and make sure an editor world is open.");
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("PublishBuildingOsmContext", "Publish Shared OSM Context"));
	AFlexOsmImportContextActor* Context = AFlexOsmImportContextActor::FindOrCreate(World);
	if (!Context)
	{
		LastResult = TEXT("Failed to create the level's shared OSM context actor.");
		return;
	}
	Context->SetContext(OsmAsset.Get(), OsmImportSettings);
	SyncBuildingGrammarGeoReference(World, Context);
	LastResult = Context->bHasResolvedOrigin
		? FString::Printf(TEXT("Published the OSM asset at shared origin %.8f, %.8f. Save the level to persist it."),
			Context->ResolvedOriginLatLon.X, Context->ResolvedOriginLatLon.Y)
		: TEXT("Published the OSM asset, but no projection origin could be resolved.");
}

void UBuildingGrammarEdModeSettings::GenerateBuildingsFromOsmAsset()
{
	UWorld* World = TargetWorld.Get();
	FString AlignmentSummary;
	if (World && bUseFlexNetworkOsmContext)
	{
		AFlexOsmImportContextActor* Context = AFlexOsmImportContextActor::Find(World);
		if (Context)
		{
			if (Context->OsmAsset)
			{
				// The shared level contract deliberately wins over stale transient mode values.
				OsmAsset = Context->OsmAsset;
				OsmImportSettings = Context->ImportSettings;
			}
		}
		else if (OsmAsset)
		{
			// Building-only workflows can establish the context; a later road import can load it.
			Context = AFlexOsmImportContextActor::FindOrCreate(World);
			if (Context)
			{
				Context->SetContext(OsmAsset.Get(), OsmImportSettings);
			}
		}
		SyncBuildingGrammarGeoReference(World, Context);
		if (Context && Context->bHasResolvedOrigin)
		{
			AlignmentSummary = FString::Printf(TEXT(" Shared FlexNetwork origin: %.8f, %.8f."),
				Context->ResolvedOriginLatLon.X, Context->ResolvedOriginLatLon.Y);
		}
	}
	if (!World || !OsmAsset)
	{
		LastResult = TEXT("Select an OSM asset and make sure an editor world is open.");
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(LastResult));
		return;
	}

	FBuildingGrammarConfig Config;
	FString Error;
	if (!ResolveConfig(Config, Error))
	{
		LastResult = FString::Printf(TEXT("Grammar config failed: %s"), *Error);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(LastResult));
		return;
	}

	TArray<FFlexOsmBuildingFootprint> FlexFootprints;
	if (!FFlexOsmBuildingFootprints::ExtractProjected(OsmAsset.Get(), OsmImportSettings, FlexFootprints, Error))
	{
		LastResult = Error;
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(LastResult));
		return;
	}
	if (FlexFootprints.IsEmpty())
	{
		LastResult = TEXT("No closed building ways or building multipolygon relations were found in the OSM asset.");
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(LastResult));
		return;
	}

	TArray<FBuildingFootprint> Footprints;
	Footprints.Reserve(FlexFootprints.Num());
	for (FFlexOsmBuildingFootprint& FlexFootprint : FlexFootprints)
	{
		FBuildingFootprint Footprint;
		Footprint.OsmId = FlexFootprint.OsmId;
		Footprint.SourceType = MoveTemp(FlexFootprint.SourceType);
		Footprint.OuterRing = MoveTemp(FlexFootprint.OuterRingMeters);
		Footprint.Tags = MoveTemp(FlexFootprint.Tags);
		Footprint.bIsBuildingPart = FlexFootprint.bIsBuildingPart;
		Footprint.Holes.Reserve(FlexFootprint.Holes.Num());
		for (FFlexOsmBuildingRing& FlexHole : FlexFootprint.Holes)
		{
			FGrammarRing Hole;
			Hole.Points = MoveTemp(FlexHole.PointsMeters);
			Footprint.Holes.Add(MoveTemp(Hole));
		}
		Footprints.Add(MoveTemp(Footprint));
	}

	const FScopedTransaction Transaction(LOCTEXT("GenerateBuildingsFromOsmAsset", "Generate Buildings From OSM Asset"));
	if (bReplaceExistingBuildingPools)
	{
		TArray<ABuildingInstancePoolActor*> ExistingPools;
		for (TActorIterator<ABuildingInstancePoolActor> It(World); It; ++It)
		{
			ExistingPools.Add(*It);
		}
		for (ABuildingInstancePoolActor* Pool : ExistingPools)
		{
			Pool->Modify();
			Pool->Destroy();
		}
	}

	const TArray<FGrammarBuildingVolume> Volumes = FBuildingPartResolver::Resolve(Footprints, Config);
	TArray<ABuildingInstancePoolActor*> Pools;
	const int32 GeneratedCount = UBuildingGenerationLibrary::GenerateBuildingsFromResolvedVolumes(
		World, Volumes, Config, Pools, CellSize, RuntimeGridName);

	LastResult = FString::Printf(TEXT("Generated %d buildings from %d OSM footprints into %d pool(s).%s Click a generated building in the viewport to customize it."),
		GeneratedCount, Footprints.Num(), Pools.Num(), *AlignmentSummary);
}

void UBuildingGrammarEdModeSettings::DeleteGeneratedBuildingPools()
{
	UWorld* World = TargetWorld.Get();
	if (!World)
	{
		LastResult = TEXT("No editor world is open.");
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteBuildingPools", "Delete Generated Building Pools"));
	int32 DeletedCount = 0;
	TArray<ABuildingInstancePoolActor*> Pools;
	for (TActorIterator<ABuildingInstancePoolActor> It(World); It; ++It)
	{
		Pools.Add(*It);
	}
	for (ABuildingInstancePoolActor* Pool : Pools)
	{
		Pool->Modify();
		if (Pool->Destroy())
		{
			++DeletedCount;
		}
	}
	LastResult = FString::Printf(TEXT("Deleted %d generated building pool(s)."), DeletedCount);
}

#undef LOCTEXT_NAMESPACE
