#pragma once

#include "CoreMinimal.h"
#include "Geometry/GrammarFace.h"
#include "MeshSpec.generated.h"

// Port of grammar.py's MeshSpec, narrowed to the roles the grammar engine actually builds real
// geometry for (see FGrammarBuildingSpec's comment): per-building-unique "hero" surfaces --
// facade walls and roof planes. Every other role instead becomes an FGrammarPlacementRecord
// (Spec/PlacementRecord.h) rather than an FGrammarMeshSpec, which is why Python's instance_count
// and export_parts fields (both artifacts of Blender's mesh-merge/instancing scheme) have no
// equivalent here -- true HISM instancing replaces that need entirely.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarMeshSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	FString Name;

	// Free-form role tag, e.g. "facade", "wall_row", "roof". See FBuildingGrammarConfig::DefaultBatchRoles
	// for the full role vocabulary used across the engine.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	FString Role;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	FString Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	FString TexturePath;

	// World-space (project-local-meters, not yet scaled to UE centimeters) vertex positions.
	// BuildingGrammarCore's grammar engine stays in meters end-to-end -- footprint coordinates and
	// every config-driven dimension (window/door/floor sizes, etc.) are meters throughout, matching
	// the Blender add-on's config.py values literally -- so the meters->centimeters conversion is
	// deferred to a single point downstream, FGrammarDynamicMeshBuilder::BuildDynamicMesh
	// (BuildingGrammarGeometry), rather than happening here at the ingestion/projection boundary.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	TArray<FVector> Vertices;

	// CCW polygonal faces indexing into Vertices; not pre-triangulated, see FGrammarFace.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	TArray<FGrammarFace> Faces;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	double TextureScale = 1.0;
};
