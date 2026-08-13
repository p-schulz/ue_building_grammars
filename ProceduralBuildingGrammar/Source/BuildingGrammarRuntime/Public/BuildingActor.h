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
// are both produced by the grammar engine as absolute-position coordinates in BuildingGrammarCore's
// working unit, meters (the OSM ingestion projection step, FLocalTangentPlaneProjection, places
// everything in local-tangent-plane meters -- see its header comment for why the meters->UE
// centimeters conversion is deliberately deferred). ApplyBuildingSpec is where that conversion
// happens: hero mesh vertices are scaled into centimeters by FGrammarDynamicMeshBuilder, and
// placement transforms are scaled into centimeters inline before being handed to the pool. Because
// the result is absolute UE-centimeter world space with no further per-building recentering, this
// actor's own transform must stay at identity (spawn it at the world origin and leave it there) --
// moving or rotating it after ApplyBuildingSpec would double-transform its DynamicMeshComponents,
// whose vertex data is already absolute. Recentering each building's geometry to be actor-relative
// (so buildings can be freely moved/streamed as independent actors) is a documented follow-up, not
// yet implemented.
UCLASS(BlueprintType)
class BUILDINGGRAMMARRUNTIME_API ABuildingActor : public AActor
{
	GENERATED_BODY()

public:
	ABuildingActor();

	// Rebuilds this actor's hero-mesh components from Spec.HeroMeshes (destroying any from a
	// previous call), and appends every Spec.Placements entry into Pool (if non-null). For each
	// hero mesh, ResolveKitMesh is NOT used (hero surfaces are unique per-building DynamicMesh
	// geometry, never a kit part) -- only ResolveMaterial(MeshSpec.StyleName, MeshSpec.Role,
	// MeshSpec.Material, MeshSpec.Color) is called. For each placement, ResolveKitMesh(Role,
	// VariantKey) resolves the shared kit mesh and ResolveMaterial(Placement.StyleName,
	// Placement.Role, Placement.VariantKey, Placement.Color) resolves its material (VariantKey
	// doubles as the material name throughout the grammar engine's placement output -- see
	// BuildingGrammarCore's GrammarPlacementHelpers.h). Pass FGrammarKitResolver::ResolveKitMesh/
	// ResolveMaterial (BuildingGrammarGeometry) for the real implementation; either callback
	// returning null is handled gracefully (no mesh/default material) rather than asserting, so
	// callers can pass no-op lambdas to skip kit rendering entirely if desired.
	void ApplyBuildingSpec(
		const FGrammarBuildingSpec& Spec,
		ABuildingInstancePoolActor* Pool,
		TFunctionRef<UStaticMesh* (const FString& Role, const FString& VariantKey)> ResolveKitMesh,
		TFunctionRef<UMaterialInterface* (const FString& StyleName, const FString& Role, const FString& MaterialName, const FLinearColor& Color)> ResolveMaterial);

	// Assigns AActor's inherited World Partition RuntimeGrid property (a named grid must also be
	// defined in the level's WP runtime hash settings for this to have any effect beyond the
	// engine default grid -- see the World Partition integration note in
	// BuildingStreamingSubsystem.h). Only meaningful for buildings generated in-editor and then
	// saved as part of the level; a purely runtime-spawned actor at play time is never partitioned
	// by WP regardless of this property. Written from recollection of AActor's RuntimeGrid member
	// name, not verified against engine headers -- check this first if it fails to compile.
	void SetBuildingRuntimeGrid(FName GridName);

private:
	UPROPERTY()
	TArray<TObjectPtr<UDynamicMeshComponent>> HeroComponents;
};
