#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Trees/TreeTypes.h"
#include "TreeMeshSettings.generated.h"

class UStaticMesh;

// Project-wide tree mesh assignment, one slot LIST per EGrammarTreeType -- same pattern as
// ProceduralRoads' URoadMaterialSettings (a project settings class, not a per-actor/per-node
// property), since a single tree import can spawn thousands of instances at once and there's no
// sensible "per-instance" place to assign a mesh. Left empty by default: ATreeInstancePoolActor
// still creates one bucket per type either way, just with no instance added for a type that has no
// mesh configured here yet -- see UTreeImportLibrary::ImportTreesFromGeoJson. A type with 2+ meshes
// gets one picked at random per tree (PickMeshForType) for visual variety within a type.
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Procedural Building Grammar - Trees"))
class BUILDINGGRAMMARRUNTIME_API UTreeMeshSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category = "Tree Meshes", meta = (DisplayName = "Fruit Tree Meshes (Obstbaum)"))
	TArray<TSoftObjectPtr<UStaticMesh>> FruitTreeMeshes;

	UPROPERTY(EditAnywhere, config, Category = "Tree Meshes", meta = (DisplayName = "Broadleaf Tree Meshes (Laubbaum)"))
	TArray<TSoftObjectPtr<UStaticMesh>> BroadleafTreeMeshes;

	UPROPERTY(EditAnywhere, config, Category = "Tree Meshes", meta = (DisplayName = "Unknown Tree Meshes (unbekannt)"))
	TArray<TSoftObjectPtr<UStaticMesh>> UnknownTreeMeshes;

	// Uniform scale range applied randomly per spawned tree instance (see PickScale /
	// UTreeImportLibrary::ImportTreesFromGeoJson) -- one shared range across every tree type, same
	// as the import's own random-yaw behavior isn't per-type either. Order doesn't matter: PickScale
	// sorts these itself, so entering them backwards in the details panel still works.
	UPROPERTY(EditAnywhere, config, Category = "Scale Variation", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "3.0"))
	double MinScale = 0.85;

	UPROPERTY(EditAnywhere, config, Category = "Scale Variation", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "3.0"))
	double MaxScale = 1.15;

	// The configured mesh slot list for Type -- "a list of available tree types" is EGrammarTreeType
	// itself (Trees/TreeTypes.h); this is the "select tree meshes for each type" half.
	const TArray<TSoftObjectPtr<UStaticMesh>>& GetMeshesForType(EGrammarTreeType Type) const;

	// Synchronously loads (LoadSynchronous) and returns one entry from GetMeshesForType(Type),
	// chosen via Stream -- or nullptr if that type has no meshes configured yet.
	UStaticMesh* PickMeshForType(EGrammarTreeType Type, FRandomStream& Stream) const;

	// Random uniform scale factor in [min(MinScale,MaxScale), max(MinScale,MaxScale)] via Stream.
	double PickScale(FRandomStream& Stream) const;
};
