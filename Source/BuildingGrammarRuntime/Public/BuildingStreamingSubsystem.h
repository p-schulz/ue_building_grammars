#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Config/BuildingGrammarConfig.h"
#include "Osm/BuildingPartResolver.h"
#include "BuildingStreamingSubsystem.generated.h"

class ABuildingActor;
class ABuildingInstancePoolActor;

// Proximity-based building streaming: loads an OSM extract once (parse + assemble + project +
// building-part resolution -- the expensive, one-time part of UBuildingGenerationLibrary's
// pipeline), buckets the resulting volumes into a grid of square cells by footprint centroid, and
// then only actually runs the grammar engine + spawns actors for cells within StreamingRadius of
// a reference location (typically the player), evicting cells that fall out of range. This is
// what satisfies docs/PLAN.md section 5's "on-demand generation, pool eviction when a cell
// unloads" -- the same FBuildingGrammarEngine/ABuildingActor/ABuildingInstancePoolActor pieces
// UBuildingGenerationLibrary uses for a one-shot editor-tool generation, just driven by distance
// instead of a button press.
//
// Deliberately simple for a first version: no async/threaded generation (cell activation runs
// synchronously on whichever frame crosses the radius, so a very large StreamingRadius or CellSize
// can cause a frame hitch -- keep cells small enough that activating one is cheap), and no
// automatic per-frame tracking of a followed actor (SetReferenceLocation must be called explicitly
// -- e.g. from a pawn's Tick, a timer, or a Blueprint event -- rather than this subsystem ticking
// itself). Both are reasonable follow-ups once there's a real scene to profile against.
UCLASS()
class BUILDINGGRAMMARRUNTIME_API UBuildingStreamingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
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
	// Tick, not necessarily every single frame.
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	void SetReferenceLocation(FVector WorldLocation);

	// Destroys every currently active cell's actors/pools (keeps the loaded volume data, so cells
	// can be reactivated later without re-parsing).
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	void DeactivateAllCells();

	int32 NumLoadedVolumes() const;
	int32 NumActiveCells() const;

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
};
