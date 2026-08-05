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
// implemented -- see docs/PLAN.md section 5.
UCLASS(BlueprintType)
class BUILDINGGRAMMARRUNTIME_API ABuildingActor : public AActor
{
	GENERATED_BODY()

public:
	ABuildingActor();

	// Rebuilds this actor's hero-mesh components from Spec.HeroMeshes (destroying any from a
	// previous call), and appends every Spec.Placements entry into Pool (if non-null) via
	// ResolveKitMesh(Role, VariantKey) -- returning null for every role is expected until kit
	// baking exists (docs/PLAN.md section 4), in which case only the hero walls/roof will render.
	// ResolveMaterial(MaterialName) resolves a mesh spec's Material string to a UMaterialInterface;
	// returning null leaves the component on the engine default material -- a parametrized master
	// material + per-style Material Instances (docs/PLAN.md section 6) is the intended real
	// resolver, not yet built.
	void ApplyBuildingSpec(
		const FGrammarBuildingSpec& Spec,
		ABuildingInstancePoolActor* Pool,
		TFunctionRef<UStaticMesh* (const FString& Role, const FString& VariantKey)> ResolveKitMesh,
		TFunctionRef<UMaterialInterface* (const FString& MaterialName)> ResolveMaterial);

private:
	UPROPERTY()
	TArray<TObjectPtr<UDynamicMeshComponent>> HeroComponents;
};
