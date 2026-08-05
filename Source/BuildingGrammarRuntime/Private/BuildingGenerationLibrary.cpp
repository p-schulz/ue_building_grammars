#include "BuildingGenerationLibrary.h"
#include "BuildingInstancePoolActor.h"
#include "Osm/OsmTypes.h"
#include "Osm/BuildingFootprintAssembler.h"
#include "Osm/BuildingPartResolver.h"
#include "Geo/LocalTangentPlaneProjection.h"
#include "Grammar/BuildingGrammarEngine.h"
#include "GrammarKitResolver.h"
#include "Engine/World.h"

namespace
{
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

int32 UBuildingGenerationLibrary::GenerateBuildingsFromOsmFile(
	const UObject* WorldContextObject,
	const FString& OsmFilePath,
	double OriginLatitude,
	double OriginLongitude,
	const FBuildingGrammarConfig& Config,
	ABuildingInstancePoolActor*& OutPool,
	FName RuntimeGridName)
{
	FOsmDocument Document;
	FString ParseError;
	if (!FOsmDocument::ParseFile(OsmFilePath, Document, ParseError))
	{
		UE_LOG(LogTemp, Warning, TEXT("UBuildingGenerationLibrary: failed to parse '%s': %s"), *OsmFilePath, *ParseError);
		return 0;
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
		ApplyMinHeightOffset(Spec, Volume.MinHeight);

		OutPool->ApplyBuildingSpec(Spec, &FGrammarKitResolver::ResolveKitMesh, &FGrammarKitResolver::ResolveMaterial);
		++GeneratedCount;
	}
	OutPool->FlushHeroMeshUpdates();

	return GeneratedCount;
}
