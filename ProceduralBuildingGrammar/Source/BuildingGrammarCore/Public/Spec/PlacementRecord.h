#pragma once

#include "CoreMinimal.h"
#include "PlacementRecord.generated.h"

// The instancing-path counterpart to FGrammarMeshSpec (see its comment). Every repeated small
// facade/roof element (window, frame, mullion, sill, ledge, balcony+rail+bar, door
// frame/handle/canopy, roof tile/edge/window, dormer, chimney, gutter, antenna+panel, PV/HVAC/
// plant, shutter/signboard/awning/garage-door, panel seam, insulation band -- the roles in
// FBuildingGrammarConfig::DefaultBatchRoles) is placement math only: which pre-baked "kit" part to
// use and where to put it. No per-instance mesh is ever constructed for these roles; the grammar
// engine emits this record and BuildingGrammarRuntime appends Transform into the
// UHierarchicalInstancedStaticMeshComponent pool keyed by (Role, VariantKey).
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarPlacementRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	FString Role;

	// Identifies which baked kit StaticMesh/HISM bucket this instance belongs to -- e.g. a style
	// id plus a quantized-dimension signature, analogous to grammar.py's mesh_cache_key. Assigned
	// by the kit-baking step (BuildingGrammarGeometry, Phase 4); left as an opaque string here so
	// the grammar engine doesn't need to know how kits are organized.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	FString VariantKey;

	// World-space (project-local-meters, not yet scaled to UE centimeters) instance transform --
	// Location and Scale (box dimensions, see FGrammarPlacementHelpers::MakeBoxPlacement) are both
	// meters. Converted to centimeters in ABuildingActor::ApplyBuildingSpec, the same
	// meters->centimeters boundary FGrammarDynamicMeshBuilder applies to hero mesh vertices.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	FTransform Transform = FTransform::Identity;

	// Per-instance tint, for kit parts whose material supports a color variant via per-instance
	// custom primitive data (e.g. grammar.py's per-building/per-facade wall-color variants).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	FLinearColor Color = FLinearColor::White;
};
