#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Config/BuildingGrammarConfig.h"
#include "BuildingGenerationLibrary.generated.h"

class ABuildingInstancePoolActor;
struct FGrammarBuildingVolume;

// The single end-to-end entry point tying every BuildingGrammarCore/Geometry/Runtime piece
// together: OSM file -> parsed document -> assembled footprints -> projected to local-tangent-plane
// meters (see FLocalTangentPlaneProjection) -> building-part parent/child resolution -> per-volume
// grammar generation -> batched into a shared ABuildingInstancePoolActor (whose ApplyBuildingSpec
// converts both hero meshes and placements to UE-centimeter world space -- see its own comment --
// and merges every building's hero surfaces and instanced kit parts into that one pool rather than
// giving each building its own ABuildingActor). Being a plain UBlueprintFunctionLibrary function
// (not a method on some Editor-only tool object) is what satisfies docs/PLAN.md's "runtime-capable
// core from day one" requirement concretely: an Editor Utility Widget can call this exact function
// from Blueprint, and so can game/runtime code -- there is no separate editor-only code path to
// keep in sync.
UCLASS()
class BUILDINGGRAMMARRUNTIME_API UBuildingGenerationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Building-part min-height offsets (grammar:part:min_height) are applied; kit mesh/material
	// resolution goes through FGrammarKitResolver (BuildingGrammarGeometry), baking the shared kit
	// assets on first use inside the editor -- see the "Packaging for a shipped build" note in the
	// plugin README.
	//
	// OriginLatitude/OriginLongitude set the projection origin (see FLocalTangentPlaneProjection);
	// pass the OSM extract's approximate center. If OutPool is null, a new
	// ABuildingInstancePoolActor is spawned and returned in it; pass an existing pool to add more
	// buildings into the same instance buckets. RuntimeGridName is optional -- if set, it's
	// assigned to the pool actor's World Partition RuntimeGrid property (see
	// ABuildingInstancePoolActor::SetBuildingRuntimeGrid; only meaningful if this call's result is
	// saved as part of a World-Partition-enabled level, not for purely runtime-spawned buildings). Returns
	// the number of buildings/building-parts successfully generated (footprints that fail --
	// excluded building value, degenerate geometry -- are skipped, not fatal to the whole call).
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	static int32 GenerateBuildingsFromOsmFile(
		const UObject* WorldContextObject,
		const FString& OsmFilePath,
		double OriginLatitude,
		double OriginLongitude,
		const FBuildingGrammarConfig& Config,
		UPARAM(ref) ABuildingInstancePoolActor*& OutPool,
		FName RuntimeGridName = NAME_None);

	// Automatically splits the OSM extract into CellSize-centimeter grid cells (see
	// FBuildingVolumeGrid, BuildingGrammarCore) and spawns one ABuildingInstancePoolActor per
	// non-empty cell instead of funneling every building into a single pool -- keeps each pool's
	// bounds/geometry bounded regardless of how large the source area is, and (once saved as part
	// of a World-Partition-enabled level) lets WP partition/stream them individually, which a
	// single giant pool never could. Otherwise identical contract to GenerateBuildingsFromOsmFile:
	// same origin/config/RuntimeGridName semantics (RuntimeGridName, if set, is assigned to every
	// spawned pool), same building-part parent/child resolution (run once on the whole unsplit
	// dataset before bucketing -- a part's parent can land in a different cell than the part
	// itself, so this must not run per-cell). Returns the total number of buildings/building-parts
	// generated across every cell.
	//
	// CellSize defaults to 100m (10000cm); tune it per call. As a starting point, keep it smaller
	// than your target level's own World Partition grid cell size (see the level's WP runtime hash
	// settings) so each generated pool lands inside a single WP cell.
	//
	// bSaveAndUnloadPerCell (default false, preserves prior behavior): when true, each cell's pool
	// is saved to its own external (One-File-Per-Actor) package -- see FBuildingActorPersistence --
	// instead of staying resident for the whole run. Memory is actually freed by periodically saving
	// and reloading the whole level (FBuildingActorPersistence::SaveAndReloadLevel) rather than
	// destroying individual actors, since World Partition treats a destroyed actor as permanently
	// deleted, not unloaded (see that class's header comment). Bounds peak memory to roughly
	// CellsPerLevelReload cells' worth of working set regardless of total dataset size, at the cost
	// of OutPools staying empty (there is nothing safe to return a pointer to across a level reload)
	// and the buildings not being visible in the currently open level until World Partition streams
	// that cell back in (proximity in PIE/packaged play, or its editor "Loaded Regions"). Requires
	// the target World to already be World-Partition-enabled -- if it isn't, this fails immediately
	// (returns 0, logs an error) rather than silently falling back to keeping everything resident,
	// since that would silently reproduce the exact out-of-memory failure this exists to prevent.
	// CellsPerLevelReload controls how many cells' pools accumulate before each (comparatively
	// expensive) save-and-reload cycle; also used as the plain per-cell save batch size. If a
	// reload ever fails (e.g. the level was never saved to disk before this call), generation stops
	// early rather than continuing to accumulate unbounded memory -- whatever was generated and
	// saved so far is kept, nothing already on disk is lost.
	//
	// bBakeToLevelPerCell (default false): when true, each cell's pool has
	// ABuildingInstancePoolActor::BakeToLevelLightweight() called on it immediately after that cell
	// finishes generating -- clears its HISM bucket/hero-mesh component data (the bulk of a
	// generated cell's memory footprint) while keeping SourceVolumes/SourceConfig, so the exact same
	// geometry regenerates automatically the next time the pool loads (see that method's own
	// comment). Reduces peak memory during a large run the same way bBakeToStaticMeshPerCell used to,
	// but synchronously and without creating any new UStaticMesh asset, deleting the pool actor, or
	// losing per-building edit/regenerate capability -- the pool stays a normal
	// ABuildingInstancePoolActor and is still added to OutPools (and still eligible for
	// bSaveAndUnloadPerCell's save/reload batching; the two combine naturally, since a
	// lightweight-baked pool has less component data to save in the first place). Can also be applied
	// after the fact to already-generated pools via the "Bake to Level (Lightweight)" Tools-menu
	// action, independent of this flag. For a permanent, editable-nowhere conversion to a plain static
	// mesh instead, use "Save to Static Meshes" (ABuildingInstancePoolActor::BakeAndReplace).
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	static int32 GenerateBuildingsFromOsmFileChunked(
		const UObject* WorldContextObject,
		const FString& OsmFilePath,
		double OriginLatitude,
		double OriginLongitude,
		const FBuildingGrammarConfig& Config,
		TArray<ABuildingInstancePoolActor*>& OutPools,
		double CellSize = 10000.0,
		FName RuntimeGridName = NAME_None,
		bool bSaveAndUnloadPerCell = false,
		int32 CellsPerLevelReload = 25,
		bool bBakeToLevelPerCell = false);

	// C++-only overload for callers that want per-cell progress reporting and cancellation (e.g. to
	// drive a cancellable FScopedSlowTask -- see
	// FBuildingGrammarEditorModule::OnGenerateFromOsmClicked). Not a UFUNCTION: TFunctionRef isn't
	// a Blueprint-compatible parameter type, hence the split from the Blueprint-facing overload
	// above, which just forwards to this one with a no-op callback. OnCellCompleted is invoked once
	// per cell, after that cell's pool is fully generated and flushed (and, if bSaveAndUnloadPerCell
	// is set, saved+unloaded), with CellsCompleted counting up to TotalCells (the number of
	// non-empty cells); returning false stops generation before the next cell starts (cells already
	// generated/flushed/saved are left in place, not rolled back).
	static int32 GenerateBuildingsFromOsmFileChunked(
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
		TFunctionRef<bool(int32 CellsCompleted, int32 TotalCells)> OnCellCompleted);

	// Shared prologue for every generation entry point in this plugin (this library's own two
	// functions, and UBuildingStreamingSubsystem::LoadOsmExtract): parses OsmFilePath, projects
	// every footprint around (OriginLatitude, OriginLongitude) (see FLocalTangentPlaneProjection),
	// and resolves building-part parent/child relationships (FBuildingPartResolver::Resolve) on the
	// full, unsplit result -- callers that go on to spatially bucket OutVolumes (see
	// FBuildingVolumeGrid) must do so with this function's output, not before it, or part
	// resolution across a would-be cell boundary breaks. Not a UFUNCTION -- plain same-module C++
	// reuse, not a Blueprint-facing entry point on its own. Returns false (logging via OutError,
	// which is also user-facing in FBuildingGrammarEditorModule's error dialog) if the file fails
	// to parse; OutVolumes is left untouched in that case.
	static bool LoadResolvedVolumesFromOsmFile(
		const FString& OsmFilePath,
		double OriginLatitude,
		double OriginLongitude,
		const FBuildingGrammarConfig& Config,
		TArray<FGrammarBuildingVolume>& OutVolumes,
		FString& OutError);

	// Generates already-projected/resolved building volumes. This is the neutral hand-off used by
	// editor integrations whose OSM source is not BuildingGrammarCore's file parser (for example a
	// FlexNetwork UOsmDataAsset). Footprints must be in BuildingGrammar's meter working space and
	// building-part resolution must already have run. Pools retain the source volumes/config so the
	// viewport building picker and per-building regeneration keep working exactly like file import.
	static int32 GenerateBuildingsFromResolvedVolumes(
		const UObject* WorldContextObject,
		const TArray<FGrammarBuildingVolume>& Volumes,
		const FBuildingGrammarConfig& Config,
		TArray<ABuildingInstancePoolActor*>& OutPools,
		double CellSize = 10000.0,
		FName RuntimeGridName = NAME_None);
};
