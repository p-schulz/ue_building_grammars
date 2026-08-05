#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingActor.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class ABuildingInstancePoolActor;
struct FGrammarBuildingSpec;

// Owns one UDynamicMeshComponent per hero mesh (facade wall/wall-row + roof plane -- see
// FGrammarBuildingSpec's comment) for a single generated building or building:part volume.
//
// Coordinate convention: FGrammarMeshSpec vertex positions and FGrammarPlacementRecord transforms
// are both produced by the grammar engine as absolute world-space coordinates (the OSM ingestion
// projection step, FLocalTangentPlaneProjection, already places everything in UE-centimeter world
// space -- there is no further per-building recentering). Because of that, this actor's own
// transform must stay at identity (spawn it at the world origin and leave it there) -- moving or
// rotating it after ApplyBuildingSpec would double-transform its DynamicMeshComponents, whose
// vertex data is already absolute. Recentering each building's geometry to be actor-relative (so
// buildings can be freely moved/streamed as independent actors) is a documented follow-up, not yet
// implemented.
UCLASS(BlueprintType)
class BUILDINGGRAMMARRUNTIME_API ABuildingActor : public AActor
{
	GENERATED_BODY()

public:
	ABuildingActor();

	// Rebuilds this actor's hero-mesh components from Spec.HeroMeshes (destroying any from a
	// previous call), and appends every Spec.Placements entry into Pool (if non-null). For each
	// hero mesh, ResolveKitMesh is NOT used (hero surfaces are unique per-building DynamicMesh
	// geometry, never a kit part) -- only ResolveMaterial(MeshSpec.Material, MeshSpec.Color) is
	// called. For each placement, ResolveKitMesh(Role, VariantKey) resolves the shared kit mesh and
	// ResolveMaterial(Placement.VariantKey, Placement.Color) resolves its material (VariantKey
	// doubles as the material name throughout the grammar engine's placement output -- see
	// BuildingGrammarCore's GrammarPlacementHelpers.h). Pass
	// FGrammarKitResolver::ResolveKitMesh/ResolveMaterial (BuildingGrammarGeometry) for the real
	// implementation; either callback returning null is handled gracefully (no mesh/default
	// material) rather than asserting, so callers can pass no-op lambdas to skip kit rendering
	// entirely if desired.
	void ApplyBuildingSpec(
		const FGrammarBuildingSpec& Spec,
		ABuildingInstancePoolActor* Pool,
		TFunctionRef<UStaticMesh* (const FString& Role, const FString& VariantKey)> ResolveKitMesh,
		TFunctionRef<UMaterialInterface* (const FString& MaterialName, const FLinearColor& Color)> ResolveMaterial);

private:
	UPROPERTY()
	TArray<TObjectPtr<UDynamicMeshComponent>> HeroComponents;
};
