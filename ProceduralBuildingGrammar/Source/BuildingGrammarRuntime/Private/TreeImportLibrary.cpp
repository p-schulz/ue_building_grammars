#include "TreeImportLibrary.h"
#include "TreeInstancePoolActor.h"
#include "TreeMeshSettings.h"
#include "Geo/GeoJsonTreeParser.h"
#include "Geo/LocalTangentPlaneProjection.h"
#include "Osm/OsmTypes.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

namespace
{
	// Same meters -> UE centimeters boundary FLocalTangentPlaneProjection's own header comment
	// documents (BuildingGrammarCore works in meters throughout; this crosses to UE units exactly
	// once, same as FGrammarDynamicMeshBuilder/ABuildingActor::ApplyBuildingSpec/
	// UPCGLoadOsmBuildingVolumesSettings all do at their own equivalent boundary).
	constexpr double MetersToUnrealUnits = 100.0;

	// Far enough above/below any plausible ground height for a city-block/district-scale extract
	// (1km) that a line trace from here always starts above the highest roof and reaches below the
	// lowest basement, without needing this function to know the scene's actual vertical extent.
	constexpr double GroundTraceHalfHeight = 100000.0;
}

ATreeInstancePoolActor* UTreeImportLibrary::ImportTreesFromGeoJson(
	UObject* WorldContextObject,
	const FString& GeoJsonFilePath,
	const FString& OsmFilePath,
	double OriginLatitude,
	double OriginLongitude,
	bool bSnapToGround,
	int32 RandomSeed,
	FString& OutError)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		OutError = TEXT("No valid World from WorldContextObject");
		return nullptr;
	}

	TArray<FGrammarTreePoint> Trees;
	if (!FGeoJsonTreeParser::ParseFile(GeoJsonFilePath, Trees, OutError))
	{
		return nullptr;
	}

	FOsmDocument OsmDocument;
	FString OsmError;
	if (!FOsmDocument::ParseFile(OsmFilePath, OsmDocument, OsmError))
	{
		OutError = FString::Printf(TEXT("Failed to parse OSM region file '%s': %s"), *OsmFilePath, *OsmError);
		return nullptr;
	}

	double MinLat = 0.0, MaxLat = 0.0, MinLon = 0.0, MaxLon = 0.0;
	if (!OsmDocument.GetBounds(MinLat, MaxLat, MinLon, MaxLon))
	{
		OutError = FString::Printf(TEXT("'%s' has no usable region (no <bounds> element and no nodes)"), *OsmFilePath);
		return nullptr;
	}

	const FLocalTangentPlaneProjection Projection(OriginLatitude, OriginLongitude);
	const UTreeMeshSettings* MeshSettings = GetDefault<UTreeMeshSettings>();
	FRandomStream Stream(RandomSeed);

	ATreeInstancePoolActor* Pool = World->SpawnActor<ATreeInstancePoolActor>();
	if (!Pool)
	{
		OutError = TEXT("Failed to spawn ATreeInstancePoolActor");
		return nullptr;
	}

	for (const FGrammarTreePoint& TreePoint : Trees)
	{
		if (TreePoint.Latitude < MinLat || TreePoint.Latitude > MaxLat || TreePoint.Longitude < MinLon || TreePoint.Longitude > MaxLon)
		{
			continue;
		}

		UStaticMesh* Mesh = MeshSettings->PickMeshForType(TreePoint.Type, Stream);
		if (!Mesh)
		{
			// This tree's type has no mesh assigned in Project Settings yet -- skip rather than
			// spawn a placeholder (see this function's own header comment).
			continue;
		}

		const FVector2D LocalMeters = Projection.ProjectToLocalMeters(FVector2D(TreePoint.Longitude, TreePoint.Latitude));
		FVector Location(LocalMeters.X * MetersToUnrealUnits, LocalMeters.Y * MetersToUnrealUnits, 0.0);

		if (bSnapToGround)
		{
			const FVector TraceStart = Location + FVector(0.0, 0.0, GroundTraceHalfHeight);
			const FVector TraceEnd = Location - FVector(0.0, 0.0, GroundTraceHalfHeight);
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic))
			{
				Location.Z = Hit.Location.Z;
			}
		}

		const double YawDegrees = Stream.FRandRange(0.0, 360.0);
		const double Scale = MeshSettings->PickScale(Stream);
		const FTransform WorldTransform(FRotator(0.0, YawDegrees, 0.0), Location, FVector(Scale));
		Pool->AddTreeInstance(Mesh, WorldTransform);
	}

	return Pool;
}
