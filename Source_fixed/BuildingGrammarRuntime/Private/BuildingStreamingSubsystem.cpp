#include "BuildingStreamingSubsystem.h"
#include "BuildingActor.h"
#include "BuildingInstancePoolActor.h"
#include "Osm/OsmTypes.h"
#include "Osm/BuildingFootprintAssembler.h"
#include "Geo/LocalTangentPlaneProjection.h"
#include "Grammar/BuildingGrammarEngine.h"
#include "Geometry/GrammarGeometry2D.h"
#include "Engine/World.h"

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

bool UBuildingStreamingSubsystem::LoadOsmExtract(
	const FString& OsmFilePath,
	double OriginLatitude,
	double OriginLongitude,
	const FBuildingGrammarConfig& Config,
	double CellSize,
	double StreamingRadius)
{
	FOsmDocument Document;
	FString ParseError;
	if (!FOsmDocument::ParseFile(OsmFilePath, Document, ParseError))
	{
		UE_LOG(LogTemp, Warning, TEXT("UBuildingStreamingSubsystem: failed to parse '%s': %s"), *OsmFilePath, *ParseError);
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

	const TArray<FGrammarBuildingVolume> Volumes = FBuildingPartResolver::Resolve(ProjectedFootprints, Config);

	DeactivateAllCells();
	Cells.Empty();
	LoadedConfig = Config;
	LoadedCellSize = FMath::Max(CellSize, 1.0);
	LoadedStreamingRadius = FMath::Max(StreamingRadius, 0.0);
	bHasReferenceLocation = false;

	for (const FGrammarBuildingVolume& Volume : Volumes)
	{
		if (Volume.Footprint.OuterRing.Num() == 0)
		{
			continue;
		}
		const FVector2D Centroid = FGrammarGeometry2D::Centroid2D(Volume.Footprint.OuterRing);
		const FIntPoint CellCoord = WorldLocationToCellCoord(FVector(Centroid.X, Centroid.Y, 0.0));
		Cells.FindOrAdd(CellCoord).Volumes.Add(Volume);
	}

	return true;
}

FIntPoint UBuildingStreamingSubsystem::WorldLocationToCellCoord(const FVector& WorldLocation) const
{
	return FIntPoint(
		static_cast<int32>(FMath::FloorToDouble(WorldLocation.X / LoadedCellSize)),
		static_cast<int32>(FMath::FloorToDouble(WorldLocation.Y / LoadedCellSize)));
}

FVector UBuildingStreamingSubsystem::CellCenter(const FIntPoint& CellCoord) const
{
	return FVector((CellCoord.X + 0.5) * LoadedCellSize, (CellCoord.Y + 0.5) * LoadedCellSize, 0.0);
}

void UBuildingStreamingSubsystem::SetReferenceLocation(FVector WorldLocation)
{
	bHasReferenceLocation = true;
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
	Cell->Pool = Pool;

	const auto NullKitMeshResolver = [](const FString&, const FString&) -> UStaticMesh* { return nullptr; };
	const auto NullMaterialResolver = [](const FString&) -> UMaterialInterface* { return nullptr; };

	for (const FGrammarBuildingVolume& Volume : Cell->Volumes)
	{
		FGrammarBuildingSpec Spec;
		FString GenerationError;
		if (!FBuildingGrammarEngine::GenerateBuildingSpec(Volume.Footprint.OuterRing, Volume.VolumeTags, LoadedConfig, Volume.SourceName, Spec, GenerationError))
		{
			continue;
		}
		ApplyMinHeightOffset(Spec, Volume.MinHeight);

		ABuildingActor* Actor = World->SpawnActor<ABuildingActor>();
		if (!Actor)
		{
			continue;
		}
		Actor->ApplyBuildingSpec(Spec, Pool, NullKitMeshResolver, NullMaterialResolver);
		Cell->SpawnedActors.Add(Actor);
	}

	Cell->bActive = true;
}

void UBuildingStreamingSubsystem::DeactivateCell(const FIntPoint& CellCoord)
{
	FStreamingCell* Cell = Cells.Find(CellCoord);
	if (!Cell || !Cell->bActive)
	{
		return;
	}

	for (const TWeakObjectPtr<ABuildingActor>& Actor : Cell->SpawnedActors)
	{
		if (ABuildingActor* ActorPtr = Actor.Get())
		{
			ActorPtr->Destroy();
		}
	}
	Cell->SpawnedActors.Empty();

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
