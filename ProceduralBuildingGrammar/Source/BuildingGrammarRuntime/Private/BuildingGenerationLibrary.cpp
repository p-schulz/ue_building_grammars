#include "BuildingGenerationLibrary.h"
#include "BuildingInstancePoolActor.h"
#include "BuildingActorPersistence.h"
#include "Osm/BuildingGrammarOsmTypes.h"
#include "Osm/BuildingFootprintAssembler.h"
#include "Osm/BuildingPartResolver.h"
#include "Osm/BuildingVolumeGrid.h"
#include "Osm/StreetNetworkAssembler.h"
#include "Osm/StreetRidgeAlignment.h"
#include "Geo/LocalTangentPlaneProjection.h"
#include "Grammar/BuildingGrammarEngine.h"
#include "GrammarKitResolver.h"
#include "Engine/World.h"

bool UBuildingGenerationLibrary::LoadResolvedVolumesFromOsmFile(
	const FString& OsmFilePath,
	double OriginLatitude,
	double OriginLongitude,
	const FBuildingGrammarConfig& Config,
	TArray<FGrammarBuildingVolume>& OutVolumes,
	FString& OutError)
{
	FOsmDocument Document;
	if (!FOsmDocument::ParseFile(OsmFilePath, Document, OutError))
	{
		return false;
	}

	const TArray<FBuildingFootprint> RawFootprints = FBuildingFootprintAssembler::Assemble(Document);

	const FLocalTangentPlaneProjection Projection(OriginLatitude, OriginLongitude);
	TArray<FBuildingFootprint> ProjectedFootprints;
	ProjectedFootprints.Reserve(RawFootprints.Num());
	for (FBuildingFootprint Footprint : RawFootprints)
	{
		Footprint.OuterRing = Projection.ProjectRing(Footprint.OuterRing);
		for (FGrammarRing& Hole : Footprint.Holes)
		{
			Hole.Points = Projection.ProjectRing(Hole.Points);
		}
		ProjectedFootprints.Add(MoveTemp(Footprint));
	}

	// Must run on the full, unsplit footprint list -- a building-part's parent can end up in a
	// different spatial cell than the part itself once a caller buckets OutVolumes (see
	// FBuildingVolumeGrid), so resolution has to happen before any such split, not per-cell.
	OutVolumes = FBuildingPartResolver::Resolve(ProjectedFootprints, Config);

	// Resolves EGrammarRidgeAlignment::ClosestStreet geometrically by injecting a synthetic
	// grammar:roof:ridge_direction tag onto each volume (see Osm/StreetRidgeAlignment.h) -- gated
	// behind ConfigNeedsStreetAlignment so configs that only ever use LongestAxis skip parsing/
	// matching streets entirely.
	if (FGrammarStreetAlignment::ConfigNeedsStreetAlignment(Config))
	{
		TArray<FGrammarStreetSegment> Streets = FStreetNetworkAssembler::Assemble(Document);
		for (FGrammarStreetSegment& Street : Streets)
		{
			Street.Points = Projection.ProjectRing(Street.Points);
		}
		FGrammarStreetAlignment::ApplyRidgeDirectionTags(OutVolumes, Streets, Config.RoofStreetAlignmentSearchRadius);
	}

	return true;
}

int32 UBuildingGenerationLibrary::GenerateBuildingsFromOsmFile(
	const UObject* WorldContextObject,
	const FString& OsmFilePath,
	double OriginLatitude,
	double OriginLongitude,
	const FBuildingGrammarConfig& Config,
	ABuildingInstancePoolActor*& OutPool,
	FName RuntimeGridName)
{
	TArray<FGrammarBuildingVolume> Volumes;
	FString LoadError;
	if (!LoadResolvedVolumesFromOsmFile(OsmFilePath, OriginLatitude, OriginLongitude, Config, Volumes, LoadError))
	{
		UE_LOG(LogTemp, Warning, TEXT("UBuildingGenerationLibrary: failed to parse '%s': %s"), *OsmFilePath, *LoadError);
		return 0;
	}

	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBuildingGenerationLibrary: no valid World from WorldContextObject"));
		return 0;
	}

	if (!OutPool)
	{
		OutPool = World->SpawnActor<ABuildingInstancePoolActor>();
		if (OutPool && RuntimeGridName != NAME_None)
		{
			OutPool->SetBuildingRuntimeGrid(RuntimeGridName);
		}
	}
	if (!OutPool)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBuildingGenerationLibrary: failed to spawn ABuildingInstancePoolActor"));
		return 0;
	}

	int32 GeneratedCount = 0;
	for (const FGrammarBuildingVolume& Volume : Volumes)
	{
		FGrammarBuildingSpec Spec;
		FString GenerationError;
		if (!FBuildingGrammarEngine::GenerateBuildingSpec(Volume.Footprint.OuterRing, Volume.VolumeTags, Config, Volume.SourceName, Spec, GenerationError))
		{
			// Previously silent -- a building could vanish (no walls, no roof, no error visible
			// anywhere) with zero trace. GenerationError was always computed by the failing check
			// inside GenerateBuildingSpec, just never logged.
			UE_LOG(LogTemp, Warning, TEXT("UBuildingGenerationLibrary: skipped building '%s': %s"), *Volume.SourceName, *GenerationError);
			continue;
		}
		FBuildingGrammarEngine::ApplyMinHeightOffset(Spec, Volume.MinHeight);

		OutPool->ApplyBuildingSpec(Spec, &FGrammarKitResolver::ResolveKitMesh, &FGrammarKitResolver::ResolveMaterial);
		++GeneratedCount;
	}
	OutPool->FlushHeroMeshUpdates();

	return GeneratedCount;
}

int32 UBuildingGenerationLibrary::GenerateBuildingsFromOsmFileChunked(
	const UObject* WorldContextObject,
	const FString& OsmFilePath,
	double OriginLatitude,
	double OriginLongitude,
	const FBuildingGrammarConfig& Config,
	TArray<ABuildingInstancePoolActor*>& OutPools,
	double CellSize,
	FName RuntimeGridName,
	bool bSaveAndUnloadPerCell,
	int32 CellsPerLevelReload,
	bool bBakeToLevelPerCell)
{
	return GenerateBuildingsFromOsmFileChunked(WorldContextObject, OsmFilePath, OriginLatitude, OriginLongitude, Config, OutPools, CellSize, RuntimeGridName, bSaveAndUnloadPerCell, CellsPerLevelReload, bBakeToLevelPerCell, [](int32, int32) { return true; });
}

int32 UBuildingGenerationLibrary::GenerateBuildingsFromResolvedVolumes(
	const UObject* WorldContextObject,
	const TArray<FGrammarBuildingVolume>& Volumes,
	const FBuildingGrammarConfig& Config,
	TArray<ABuildingInstancePoolActor*>& OutPools,
	double CellSize,
	FName RuntimeGridName)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBuildingGenerationLibrary: no valid World from WorldContextObject"));
		return 0;
	}

	int32 GeneratedCount = 0;
	const TMap<FIntPoint, TArray<FGrammarBuildingVolume>> Buckets =
		FBuildingVolumeGrid::BucketByCell(Volumes, FMath::Max(CellSize, 100.0));

	for (const TPair<FIntPoint, TArray<FGrammarBuildingVolume>>& CellPair : Buckets)
	{
		ABuildingInstancePoolActor* Pool = World->SpawnActor<ABuildingInstancePoolActor>();
		if (!Pool)
		{
			UE_LOG(LogTemp, Warning, TEXT("UBuildingGenerationLibrary: failed to spawn pool for cell (%d, %d)"), CellPair.Key.X, CellPair.Key.Y);
			continue;
		}
		if (RuntimeGridName != NAME_None)
		{
			Pool->SetBuildingRuntimeGrid(RuntimeGridName);
		}

		Pool->SourceVolumes = CellPair.Value;
		Pool->SourceConfig = Config;

		int32 PoolGeneratedCount = 0;
		for (const FGrammarBuildingVolume& Volume : CellPair.Value)
		{
			FGrammarBuildingSpec Spec;
			FString GenerationError;
			if (!FBuildingGrammarEngine::GenerateBuildingSpec(
				Volume.Footprint.OuterRing, Volume.VolumeTags, Config, Volume.SourceName, Spec, GenerationError))
			{
				UE_LOG(LogTemp, Warning, TEXT("UBuildingGenerationLibrary: skipped building '%s': %s"), *Volume.SourceName, *GenerationError);
				continue;
			}

			FBuildingGrammarEngine::ApplyMinHeightOffset(Spec, Volume.MinHeight);
			Pool->ApplyBuildingSpec(Spec, &FGrammarKitResolver::ResolveKitMesh, &FGrammarKitResolver::ResolveMaterial);
			++PoolGeneratedCount;
		}

		Pool->FlushHeroMeshUpdates();
		if (PoolGeneratedCount > 0)
		{
			OutPools.Add(Pool);
			GeneratedCount += PoolGeneratedCount;
		}
		else
		{
			Pool->Destroy();
		}
	}

	return GeneratedCount;
}

int32 UBuildingGenerationLibrary::GenerateBuildingsFromOsmFileChunked(
	const UObject* WorldContextObject,
	const FString& OsmFilePath,
	double OriginLatitude,
	double OriginLongitude,
	const FBuildingGrammarConfig& Config,
	TArray<ABuildingInstancePoolActor*>& OutPools,
	double CellSize,
	FName RuntimeGridName,
	bool bSaveAndUnloadPerCell,
	int32 CellsPerLevelReload,
	bool bBakeToLevelPerCell,
	TFunctionRef<bool(int32 CellsCompleted, int32 TotalCells)> OnCellCompleted)
{
	TArray<FGrammarBuildingVolume> Volumes;
	FString LoadError;
	if (!LoadResolvedVolumesFromOsmFile(OsmFilePath, OriginLatitude, OriginLongitude, Config, Volumes, LoadError))
	{
		UE_LOG(LogTemp, Warning, TEXT("UBuildingGenerationLibrary: failed to parse '%s': %s"), *OsmFilePath, *LoadError);
		return 0;
	}

	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBuildingGenerationLibrary: no valid World from WorldContextObject"));
		return 0;
	}

	// Refuse rather than silently fall back to keeping everything resident -- the caller explicitly
	// asked for the memory-bounded mode specifically to avoid the out-of-memory failure that
	// silently falling back here would just reproduce. See FBuildingActorPersistence's comment for
	// why World Partition is a hard precondition, not a soft one.
	if (bSaveAndUnloadPerCell && !FBuildingActorPersistence::IsWorldPartitioned(World))
	{
		UE_LOG(LogTemp, Error, TEXT("UBuildingGenerationLibrary: bSaveAndUnloadPerCell requires a World-Partition-enabled level; '%s' is not one"), *World->GetName());
		return 0;
	}

	const TMap<FIntPoint, TArray<FGrammarBuildingVolume>> Buckets = FBuildingVolumeGrid::BucketByCell(Volumes, FMath::Max(CellSize, 100.0));
	const int32 TotalCells = Buckets.Num();
	const int32 BatchSize = FMath::Max(CellsPerLevelReload, 1);

	// Batches several cells' pools into one FBuildingActorPersistence::SaveActors call rather than
	// saving one at a time -- see that function's header comment for why a per-actor
	// UPackage::SavePackage call turned out not to reliably register with World Partition, and why
	// this batched, FEditorFileUtils-driven approach replaced it. Memory is freed by periodically
	// saving and reloading the whole level (SaveAndReloadLevel), not by destroying actors -- see
	// FBuildingActorPersistence's header comment for why DestroyActor is unsafe here. World is
	// captured by reference since a reload reassigns it to a freshly loaded UWorld*; every pointer
	// spawned before a reload is stale afterward.
	TArray<AActor*> PendingSave;
	auto FlushPendingSave = [&PendingSave, &OutPools]()
	{
		if (PendingSave.IsEmpty())
		{
			return;
		}
		TArray<AActor*> FailedActors;
		FBuildingActorPersistence::SaveActors(PendingSave, FailedActors);
		for (AActor* FailedActor : FailedActors)
		{
			if (ABuildingInstancePoolActor* FailedPool = Cast<ABuildingInstancePoolActor>(FailedActor))
			{
				OutPools.Add(FailedPool);
			}
		}
		PendingSave.Reset();
	};

	// World is captured by reference since a successful reload reassigns it to a freshly loaded
	// UWorld*; every AActor*/UWorld* obtained before a reload is stale afterward.
	bool bReloadFailed = false;
	auto FlushPendingSaveAndReload = [&World, &FlushPendingSave, &bReloadFailed]()
	{
		FlushPendingSave();
		if (!FBuildingActorPersistence::SaveAndReloadLevel(World))
		{
			UE_LOG(LogTemp, Error, TEXT("UBuildingGenerationLibrary: failed to save and reload the level to free memory; stopping generation early (content generated and saved so far is kept)"));
			bReloadFailed = true;
		}
	};

	int32 GeneratedCount = 0;
	int32 CellsCompleted = 0;
	for (const TPair<FIntPoint, TArray<FGrammarBuildingVolume>>& CellPair : Buckets)
	{
		ABuildingInstancePoolActor* Pool = World->SpawnActor<ABuildingInstancePoolActor>();
		if (!Pool)
		{
			UE_LOG(LogTemp, Warning, TEXT("UBuildingGenerationLibrary: failed to spawn ABuildingInstancePoolActor for cell (%d, %d)"), CellPair.Key.X, CellPair.Key.Y);
			++CellsCompleted;
			if (!OnCellCompleted(CellsCompleted, TotalCells))
			{
				break;
			}
			continue;
		}
		if (RuntimeGridName != NAME_None)
		{
			Pool->SetBuildingRuntimeGrid(RuntimeGridName);
		}

		// Kept on every pool, including a lightweight-baked one -- unlike the old static-mesh bake
		// path, BakeToLevelLightweight relies on this data still being here to regenerate from later
		// (see ABuildingInstancePoolActor::RegenerateFromSource).
		Pool->SourceVolumes = CellPair.Value;
		Pool->SourceConfig = Config;

		for (const FGrammarBuildingVolume& Volume : CellPair.Value)
		{
			FGrammarBuildingSpec Spec;
			FString GenerationError;
			if (!FBuildingGrammarEngine::GenerateBuildingSpec(Volume.Footprint.OuterRing, Volume.VolumeTags, Config, Volume.SourceName, Spec, GenerationError))
			{
				// See the identical log in GenerateBuildingsFromOsmFile above -- this is the
				// chunked variant the editor's "Generate Buildings from OSM..." menu action uses.
				UE_LOG(LogTemp, Warning, TEXT("UBuildingGenerationLibrary: skipped building '%s': %s"), *Volume.SourceName, *GenerationError);
				continue;
			}
			FBuildingGrammarEngine::ApplyMinHeightOffset(Spec, Volume.MinHeight);

			Pool->ApplyBuildingSpec(Spec, &FGrammarKitResolver::ResolveKitMesh, &FGrammarKitResolver::ResolveMaterial);
			++GeneratedCount;
		}
		Pool->FlushHeroMeshUpdates();

		// Synchronous and cheap (just clears component data, no mesh-merge/Nanite/SavePackage work),
		// unlike the old static-mesh-per-cell bake this replaced -- no background pipelining needed.
		// Pool survives (see BakeToLevelLightweight's own comment), so it flows into the ordinary
		// save/return routing below exactly like an unbaked pool.
		if (bBakeToLevelPerCell)
		{
			Pool->BakeToLevelLightweight();
		}

		if (bSaveAndUnloadPerCell)
		{
			PendingSave.Add(Pool);
			if (PendingSave.Num() >= BatchSize)
			{
				FlushPendingSaveAndReload();
			}
		}
		else
		{
			OutPools.Add(Pool);
		}

		++CellsCompleted;
		if (bReloadFailed || !OnCellCompleted(CellsCompleted, TotalCells))
		{
			break;
		}
	}

	// Final partial batch: save whatever's left, but don't pay for a reload nobody needs anymore --
	// the run is ending regardless. Still save the level itself so this last batch is guaranteed on
	// disk even though nothing triggers a reload after it.
	FlushPendingSave();
	if (bSaveAndUnloadPerCell && !bReloadFailed)
	{
		FBuildingActorPersistence::SaveLevel();
	}

	return GeneratedCount;
}
