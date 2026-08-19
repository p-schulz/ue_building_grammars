#include "BuildingStreamingSubsystem.h"
#include "BuildingInstancePoolActor.h"
#include "BuildingGenerationLibrary.h"
#include "Osm/BuildingVolumeGrid.h"
#include "Grammar/BuildingGrammarEngine.h"
#include "GrammarKitResolver.h"
#include "Engine/World.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace
{
	// Duplicated from BuildingGenerationLibrary.cpp -- small enough (and specific enough to each
	// caller's spawn flow) not to warrant a shared header for a single four-line helper.
	void ApplyMinHeightOffset(FGrammarBuildingSpec& Spec, double MinHeight)
	{
		if (FMath::IsNearlyZero(MinHeight))
		{
			return;
		}
		for (FGrammarMeshSpec& Mesh : Spec.HeroMeshes)
		{
			for (FVector& Vertex : Mesh.Vertices)
			{
				Vertex.Z += MinHeight;
			}
		}
		for (FGrammarPlacementRecord& Placement : Spec.Placements)
		{
			FVector Location = Placement.Transform.GetLocation();
			Location.Z += MinHeight;
			Placement.Transform.SetLocation(Location);
		}
	}
}

void UBuildingStreamingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Harmless no-op if the level has no World Partition (GetSubsystem returns null) -- see this
	// class's header comment for why this subsystem registers as a streaming source at all.
	if (UWorld* World = GetWorld())
	{
		if (UWorldPartitionSubsystem* WorldPartitionSubsystem = World->GetSubsystem<UWorldPartitionSubsystem>())
		{
			WorldPartitionSubsystem->RegisterStreamingSourceProvider(this);
		}
	}
}

void UBuildingStreamingSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UWorldPartitionSubsystem* WorldPartitionSubsystem = World->GetSubsystem<UWorldPartitionSubsystem>())
		{
			WorldPartitionSubsystem->UnregisterStreamingSourceProvider(this);
		}
	}

	Super::Deinitialize();
}

bool UBuildingStreamingSubsystem::GetStreamingSources(TArray<FWorldPartitionStreamingSource>& OutStreamingSources) const
{
	if (!bHasReferenceLocation)
	{
		return false;
	}

	FWorldPartitionStreamingSource Source;
	Source.Name = TEXT("ProceduralBuildingGrammar");
	Source.Location = LastReferenceLocation;
	Source.Rotation = FRotator::ZeroRotator;
	Source.TargetState = EStreamingSourceTargetState::Activated;
	OutStreamingSources.Add(Source);
	return true;
}

bool UBuildingStreamingSubsystem::LoadOsmExtract(
	const FString& OsmFilePath,
	double OriginLatitude,
	double OriginLongitude,
	const FBuildingGrammarConfig& Config,
	double CellSize,
	double StreamingRadius)
{
	TArray<FGrammarBuildingVolume> Volumes;
	FString LoadError;
	if (!UBuildingGenerationLibrary::LoadResolvedVolumesFromOsmFile(OsmFilePath, OriginLatitude, OriginLongitude, Config, Volumes, LoadError))
	{
		UE_LOG(LogTemp, Warning, TEXT("UBuildingStreamingSubsystem: failed to parse '%s': %s"), *OsmFilePath, *LoadError);
		return false;
	}

	DeactivateAllCells();
	Cells.Empty();
	LoadedConfig = Config;
	LoadedCellSize = FMath::Max(CellSize, 1.0);
	LoadedStreamingRadius = FMath::Max(StreamingRadius, 0.0);
	bHasReferenceLocation = false;

	// CellSize/StreamingRadius and the WorldLocation passed to SetReferenceLocation are ordinary
	// UE-centimeter world positions (e.g. a player pawn's actual GetActorLocation()) -- see
	// FBuildingVolumeGrid::BucketByCell's comment for the meters->centimeters conversion applied
	// purely for cell bucketing (the volumes themselves stay in meters).
	for (const TPair<FIntPoint, TArray<FGrammarBuildingVolume>>& Bucket : FBuildingVolumeGrid::BucketByCell(Volumes, LoadedCellSize))
	{
		Cells.FindOrAdd(Bucket.Key).Volumes = Bucket.Value;
	}

	return true;
}

FIntPoint UBuildingStreamingSubsystem::WorldLocationToCellCoord(const FVector& WorldLocation) const
{
	return FBuildingVolumeGrid::WorldLocationToCellCoord(WorldLocation, LoadedCellSize);
}

FVector UBuildingStreamingSubsystem::CellCenter(const FIntPoint& CellCoord) const
{
	return FBuildingVolumeGrid::CellCenter(CellCoord, LoadedCellSize);
}

void UBuildingStreamingSubsystem::SetReferenceLocation(FVector WorldLocation)
{
	bHasReferenceLocation = true;
	LastReferenceLocation = WorldLocation;
	const FIntPoint CenterCoord = WorldLocationToCellCoord(WorldLocation);
	const int32 CellRadius = static_cast<int32>(FMath::CeilToDouble(LoadedStreamingRadius / LoadedCellSize)) + 1;

	TSet<FIntPoint> DesiredActive;
	for (int32 DX = -CellRadius; DX <= CellRadius; ++DX)
	{
		for (int32 DY = -CellRadius; DY <= CellRadius; ++DY)
		{
			const FIntPoint CellCoord(CenterCoord.X + DX, CenterCoord.Y + DY);
			if (!Cells.Contains(CellCoord))
			{
				continue;
			}
			if (FVector::Dist2D(WorldLocation, CellCenter(CellCoord)) <= LoadedStreamingRadius)
			{
				DesiredActive.Add(CellCoord);
			}
		}
	}

	for (TPair<FIntPoint, FStreamingCell>& CellPair : Cells)
	{
		if (CellPair.Value.bActive && !DesiredActive.Contains(CellPair.Key))
		{
			DeactivateCell(CellPair.Key);
		}
	}
	for (const FIntPoint& CellCoord : DesiredActive)
	{
		if (!Cells[CellCoord].bActive)
		{
			ActivateCell(CellCoord);
		}
	}
}

void UBuildingStreamingSubsystem::ActivateCell(const FIntPoint& CellCoord)
{
	FStreamingCell* Cell = Cells.Find(CellCoord);
	UWorld* World = GetWorld();
	if (!Cell || Cell->bActive || !World)
	{
		return;
	}

	ABuildingInstancePoolActor* Pool = World->SpawnActor<ABuildingInstancePoolActor>();
	if (!Pool)
	{
		return;
	}
	Cell->Pool = Pool;

	for (const FGrammarBuildingVolume& Volume : Cell->Volumes)
	{
		FGrammarBuildingSpec Spec;
		FString GenerationError;
		if (!FBuildingGrammarEngine::GenerateBuildingSpec(Volume.Footprint.OuterRing, Volume.VolumeTags, LoadedConfig, Volume.SourceName, Spec, GenerationError))
		{
			// See BuildingGenerationLibrary.cpp's identical log -- this was previously silent.
			UE_LOG(LogTemp, Warning, TEXT("UBuildingStreamingSubsystem: skipped building '%s': %s"), *Volume.SourceName, *GenerationError);
			continue;
		}
		ApplyMinHeightOffset(Spec, Volume.MinHeight);

		Pool->ApplyBuildingSpec(Spec, &FGrammarKitResolver::ResolveKitMesh, &FGrammarKitResolver::ResolveMaterial);
	}
	Pool->FlushHeroMeshUpdates();

	Cell->bActive = true;
}

void UBuildingStreamingSubsystem::DeactivateCell(const FIntPoint& CellCoord)
{
	FStreamingCell* Cell = Cells.Find(CellCoord);
	if (!Cell || !Cell->bActive)
	{
		return;
	}

	if (ABuildingInstancePoolActor* PoolPtr = Cell->Pool.Get())
	{
		PoolPtr->Destroy();
	}
	Cell->Pool = nullptr;

	Cell->bActive = false;
}

void UBuildingStreamingSubsystem::DeactivateAllCells()
{
	for (TPair<FIntPoint, FStreamingCell>& CellPair : Cells)
	{
		if (CellPair.Value.bActive)
		{
			DeactivateCell(CellPair.Key);
		}
	}
}

int32 UBuildingStreamingSubsystem::NumLoadedVolumes() const
{
	int32 Count = 0;
	for (const TPair<FIntPoint, FStreamingCell>& CellPair : Cells)
	{
		Count += CellPair.Value.Volumes.Num();
	}
	return Count;
}

int32 UBuildingStreamingSubsystem::NumActiveCells() const
{
	int32 Count = 0;
	for (const TPair<FIntPoint, FStreamingCell>& CellPair : Cells)
	{
		if (CellPair.Value.bActive)
		{
			++Count;
		}
	}
	return Count;
}
