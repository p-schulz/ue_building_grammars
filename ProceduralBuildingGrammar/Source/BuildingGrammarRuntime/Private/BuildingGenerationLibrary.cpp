#include "BuildingGenerationLibrary.h"
#include "BuildingInstancePoolActor.h"
#include "BuildingActorPersistence.h"
#include "Osm/OsmTypes.h"
#include "Osm/BuildingFootprintAssembler.h"
#include "Osm/BuildingPartResolver.h"
#include "Osm/BuildingVolumeGrid.h"
#include "Osm/StreetNetworkAssembler.h"
#include "Osm/StreetRidgeAlignment.h"
#include "Geo/LocalTangentPlaneProjection.h"
#include "Grammar/BuildingGrammarEngine.h"
#include "GrammarKitResolver.h"
#include "Engine/World.h"
#include "Async/Async.h"
#include "MeshDescription.h"

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
	bool bBakeToStaticMeshPerCell)
{
	return GenerateBuildingsFromOsmFileChunked(WorldContextObject, OsmFilePath, OriginLatitude, OriginLongitude, Config, OutPools, CellSize, RuntimeGridName, bSaveAndUnloadPerCell, CellsPerLevelReload, bBakeToStaticMeshPerCell, [](int32, int32) { return true; });
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
	bool bBakeToStaticMeshPerCell,
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

	// Single-slot pipeline for bBakeToStaticMeshPerCell: at most one cell's bake is ever "in flight"
	// at a time. A cell's expensive, UObject-free merge work (BuildBakedMeshDescription) runs on a
	// background task while the NEXT cell generates; the actual asset save/actor swap (must stay on
	// the game thread -- see FinalizePendingBake) happens once that merge is ready, interleaved with
	// generation rather than blocking it. PoolActor is kept alive (not yet destroyed) the whole time
	// a bake is pending, so it must never survive a level reload -- see FlushPendingSaveAndReload and
	// the barrier after the main loop below.
	struct FPendingBake
	{
		ABuildingInstancePoolActor* PoolActor = nullptr;
		FString PackagePath;
		TArray<TObjectPtr<UMaterialInterface>> BakedMaterials;
		TFuture<FMeshDescription> MergeFuture;
	};
	TOptional<FPendingBake> PendingBake;

	auto FinalizePendingBake = [&PendingBake]()
	{
		if (!PendingBake.IsSet())
		{
			return;
		}
		FMeshDescription MeshDescription = PendingBake->MergeFuture.Get(); // blocks only if not already done
		if (UStaticMesh* BakedMesh = ABuildingInstancePoolActor::FinalizeBakedAsset(
			    MoveTemp(MeshDescription), PendingBake->BakedMaterials, PendingBake->PackagePath))
		{
			ABuildingInstancePoolActor::ReplaceWithBakedAsset(PendingBake->PoolActor, BakedMesh);
		}
		PendingBake.Reset();
	};

	// World is captured by reference since a successful reload reassigns it to a freshly loaded
	// UWorld*; every AActor*/UWorld* obtained before a reload is stale afterward.
	bool bReloadFailed = false;
	auto FlushPendingSaveAndReload = [&World, &FlushPendingSave, &bReloadFailed, &FinalizePendingBake]()
	{
		// No pending bake may survive a level reload -- it still holds a live PoolActor pointer
		// waiting to be destroyed, and a reload destroys the entire UWorld out from under it.
		FinalizePendingBake();
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

		// Only worth keeping around for pools that might later be regenerated on demand (see
		// ABuildingInstancePoolActor::RegenerateFromSource) -- a baked cell is destroyed and replaced
		// by a plain AStaticMeshActor before this function returns, so there would be nothing left to
		// regenerate; skip the (otherwise harmless) copy for that path.
		if (!bBakeToStaticMeshPerCell)
		{
			Pool->SourceVolumes = CellPair.Value;
			Pool->SourceConfig = Config;
		}

		for (const FGrammarBuildingVolume& Volume : CellPair.Value)
		{
			FGrammarBuildingSpec Spec;
			FString GenerationError;
			if (!FBuildingGrammarEngine::GenerateBuildingSpec(Volume.Footprint.OuterRing, Volume.VolumeTags, Config, Volume.SourceName, Spec, GenerationError))
			{
				continue;
			}
			FBuildingGrammarEngine::ApplyMinHeightOffset(Spec, Volume.MinHeight);

			Pool->ApplyBuildingSpec(Spec, &FGrammarKitResolver::ResolveKitMesh, &FGrammarKitResolver::ResolveMaterial);
			++GeneratedCount;
		}
		Pool->FlushHeroMeshUpdates();

		// Kick off this cell's background merge before finalizing the PREVIOUS cell's pending bake,
		// so both run concurrently for a stretch (the new merge in the background, the previous
		// cell's Nanite build/SavePackage/actor swap on the game thread) before generation moves on
		// to the next cell -- see the pipeline comment above FlushPendingSaveAndReload. Pool is
		// destroyed once its own bake is eventually finalized, so there is nothing left of the
		// original ABuildingInstancePoolActor for the batching below to save/return -- skip it
		// entirely for a baked cell.
		bool bPoolReplacedByBake = false;
		if (bBakeToStaticMeshPerCell)
		{
			FBuildingBakeExtractedData Data = Pool->ExtractBakeData();
			TArray<TObjectPtr<UMaterialInterface>> BakedMaterials = Data.BakedMaterials;
			TFuture<FMeshDescription> MergeFuture = Async(EAsyncExecution::TaskGraph,
				[Data = MoveTemp(Data)]() { return ABuildingInstancePoolActor::BuildBakedMeshDescription(Data); });

			FinalizePendingBake();
			PendingBake.Emplace();
			PendingBake->PoolActor = Pool;
			PendingBake->PackagePath = Pool->MakeDefaultBakedAssetPath();
			PendingBake->BakedMaterials = MoveTemp(BakedMaterials);
			PendingBake->MergeFuture = MoveTemp(MergeFuture);
			bPoolReplacedByBake = true;
		}

		if (!bPoolReplacedByBake)
		{
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
		}

		++CellsCompleted;
		if (bReloadFailed || !OnCellCompleted(CellsCompleted, TotalCells))
		{
			break;
		}
	}

	// Nothing may be left mid-flight when this function returns -- finalize any bake still pending
	// before the final save (same reasoning as FlushPendingSaveAndReload's own barrier).
	FinalizePendingBake();

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
