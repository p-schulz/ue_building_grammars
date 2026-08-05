#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Config/BuildingGrammarConfig.h"
#include "Osm/BuildingPartResolver.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "BuildingStreamingSubsystem.generated.h"

class ABuildingActor;
class ABuildingInstancePoolActor;

// Proximity-based building streaming: loads an OSM extract once (parse + assemble + project +
// building-part resolution -- the expensive, one-time part of UBuildingGenerationLibrary's
// pipeline), buckets the resulting volumes into a grid of square cells by footprint centroid, and
// then only actually runs the grammar engine + spawns actors for cells within StreamingRadius of
// a reference location (typically the player), evicting cells that fall out of range. This is
// the same FBuildingGrammarEngine/ABuildingActor/ABuildingInstancePoolActor pipeline
// UBuildingGenerationLibrary uses for a one-shot editor-tool generation, just driven by distance
// instead of a button press.
//
// World Partition integration: this subsystem is its own independent grid, unaware of the level's
// World Partition cells by default -- but it also implements IWorldPartitionStreamingSourceProvider
// and self-registers with UWorldPartitionSubsystem (if the level has one; harmless no-op
// otherwise), so every SetReferenceLocation call also feeds WP's own native streaming with the
// same point. That means a level combining World-Partition-managed static content (buildings baked
// into the level via UBuildingGenerationLibrary and saved, which WP partitions into cells like any
// other placed actor once saved) with this subsystem's dynamically-spawned/streamed buildings gets
// consistent streaming behavior from one reference point, rather than two uncoordinated systems.
// See ABuildingActor's RuntimeGrid property for assigning editor-baked buildings to a specific WP
// runtime grid. Data Layer assignment is not implemented -- the Data Layer C++ API has changed
// shape more than once across UE5 versions and this wasn't confident enough to include; a
// documented follow-up, not a silent gap.
//
// Deliberately simple otherwise: no async/threaded generation (cell activation runs synchronously
// on whichever frame crosses the radius, so a very large StreamingRadius or CellSize can cause a
// frame hitch -- keep cells small enough that activating one is cheap), and no automatic per-frame
// tracking of a followed actor (SetReferenceLocation must be called explicitly -- e.g. from a
// pawn's Tick, a timer, or a Blueprint event -- rather than this subsystem ticking itself). Both
// are reasonable follow-ups once there's a real scene to profile against.
UCLASS()
class BUILDINGGRAMMARRUNTIME_API UBuildingStreamingSubsystem : public UWorldSubsystem, public IWorldPartitionStreamingSourceProvider
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Parses, assembles, projects, and resolves OsmFilePath's building-part volumes, then buckets
	// them into CellSize-centimeter grid cells. Does not spawn anything yet -- call
	// SetReferenceLocation to activate the cells around a starting point. Returns false (and
	// leaves any previously loaded extract in place) if the file fails to parse.
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	bool LoadOsmExtract(
		const FString& OsmFilePath,
		double OriginLatitude,
		double OriginLongitude,
		const FBuildingGrammarConfig& Config,
		double CellSize = 20000.0,
		double StreamingRadius = 60000.0);

	// Activates every loaded cell whose center is within StreamingRadius of WorldLocation
	// (generating + spawning it if it wasn't already active) and deactivates every previously
	// active cell that has fallen outside that radius (destroying its actors/pool). Call this
	// whenever the reference point has moved meaningfully -- e.g. once per second from a pawn's
	// Tick, not necessarily every single frame. Also becomes this subsystem's
	// IWorldPartitionStreamingSourceProvider position -- see this class's header comment.
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	void SetReferenceLocation(FVector WorldLocation);

	// Destroys every currently active cell's actors/pools (keeps the loaded volume data, so cells
	// can be reactivated later without re-parsing).
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	void DeactivateAllCells();

	int32 NumLoadedVolumes() const;
	int32 NumActiveCells() const;

	// IWorldPartitionStreamingSourceProvider -- appends one streaming source at the last
	// SetReferenceLocation point (nothing if SetReferenceLocation has never been called). This is
	// what makes this subsystem's proximity tracking also drive the level's native World Partition
	// streaming, not just this plugin's own cell grid.
	virtual FName GetStreamingSourceProviderName() const override;
	virtual bool GetStreamingSources(TArray<FWorldPartitionStreamingSource>& OutStreamingSources) const override;

private:
	struct FStreamingCell
	{
		TArray<FGrammarBuildingVolume> Volumes;
		bool bActive = false;
		TArray<TWeakObjectPtr<ABuildingActor>> SpawnedActors;
		TWeakObjectPtr<ABuildingInstancePoolActor> Pool;
	};

	FIntPoint WorldLocationToCellCoord(const FVector& WorldLocation) const;
	FVector CellCenter(const FIntPoint& CellCoord) const;

	void ActivateCell(const FIntPoint& CellCoord);
	void DeactivateCell(const FIntPoint& CellCoord);

	TMap<FIntPoint, FStreamingCell> Cells;
	FBuildingGrammarConfig LoadedConfig;
	double LoadedCellSize = 20000.0;
	double LoadedStreamingRadius = 60000.0;
	bool bHasReferenceLocation = false;
	FVector LastReferenceLocation = FVector::ZeroVector;
};
