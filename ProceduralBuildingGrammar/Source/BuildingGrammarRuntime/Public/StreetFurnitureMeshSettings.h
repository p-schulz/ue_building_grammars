#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "StreetFurnitureMeshSettings.generated.h"

class UStaticMesh;

// One street-furniture category's configured mesh slot list -- same "list, not single mesh" shape as
// UTreeMeshSettings' own per-type properties (visual variety: a category with 2+ meshes gets one
// picked at random per instance).
USTRUCT()
struct FStreetFurnitureMeshEntry
{
	GENERATED_BODY()

	// Matches UPCGLoadOsmPointFeaturesSettings::FStreetFurnitureCategoryConfig::Name /
	// UPCGPlaceStreetLightsAlongLitRoadsSettings' "StreetLight" category value (case-insensitive
	// lookup -- see PickMeshForCategory).
	UPROPERTY(EditAnywhere, config, Category = "Street Furniture Meshes")
	FString Category;

	UPROPERTY(EditAnywhere, config, Category = "Street Furniture Meshes")
	TArray<TSoftObjectPtr<UStaticMesh>> Meshes;
};

// Project-wide street-furniture mesh assignment, one slot LIST per category NAME (a plain array of
// {Category, Meshes} entries, not a fixed enum) -- same project-settings-level convention as
// UTreeMeshSettings, but string-keyed rather than enum-keyed, since the furniture category list is
// open-ended and user-editable per UPCGLoadOsmPointFeaturesSettings::Categories rather than a fixed,
// small set (adding a new category here is a data change, not a new UPROPERTY + switch case).
//
// A category with no entry here (or an entry with an empty Meshes list) simply gets no MeshOverride
// filled in by the PCG nodes that consult this -- left for a user to wire a different Static Mesh
// Spawner attribute mapping, not an error.
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Procedural Building Grammar - Street Furniture"))
class BUILDINGGRAMMARRUNTIME_API UStreetFurnitureMeshSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category = "Street Furniture Meshes")
	TArray<FStreetFurnitureMeshEntry> Categories;

	// Case-insensitive lookup by Category name. Synchronously loads (LoadSynchronous) and returns one
	// entry chosen via Stream -- or nullptr if that category isn't configured here, or is configured
	// with an empty mesh list.
	UStaticMesh* PickMeshForCategory(const FString& Category, FRandomStream& Stream) const;
};
