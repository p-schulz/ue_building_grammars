#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingInstancePoolActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UDynamicMeshComponent;
class UStaticMesh;
class UMaterialInterface;
struct FGrammarBuildingSpec;
namespace UE { namespace Geometry { class FDynamicMesh3; } }

// Owns one UHierarchicalInstancedStaticMeshComponent per distinct (Role, VariantKey) bucket and
// lets any number of ABuildingActors append instance transforms into the bucket that matches their
// FGrammarPlacementRecords -- see docs/PLAN.md section 5. This is the direct fix for the Python
// add-on's per-object-even-if-shared mesh limitation: every window/ledge/roof-tile/antenna across
// every building that shares this pool and resolves to the same kit part becomes one GPU-instanced
// draw, not one Blender object per instance.
//
// One pool per streaming cell/district is the intended granularity (see docs/PLAN.md section 5 --
// "owned by a per-cell manager, not per-building"); nothing here enforces that grouping, it's a
// placement decision for whatever spawns these (see BuildingGenerationLibrary.h).
UCLASS(BlueprintType)
class BUILDINGGRAMMARRUNTIME_API ABuildingInstancePoolActor : public AActor
{
	GENERATED_BODY()

public:
	ABuildingInstancePoolActor();

	// Appends one instance transform to the bucket for (Role, VariantKey), creating the bucket's
	// HISM component (and assigning it Mesh + Material) the first time that (Role, VariantKey)
	// pair is seen. Mesh/Material must be the same every time for a given (Role, VariantKey) pair
	// -- they are only actually consulted on first use; a mismatch on a later call is silently
	// ignored (matches HISM's own single-mesh/single-material-set-per-component constraint).
	// No-ops if Mesh is null (no baked kit mesh to resolve VariantKey to -- see
	// GrammarKitResolver.h in BuildingGrammarGeometry); Material may be null (component keeps
	// its mesh's default material).
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	void AddInstance(const FString& RoleTag, const FString& VariantKey, const FTransform& Transform, UStaticMesh* Mesh, UMaterialInterface* Material = nullptr);

	// Removes every instance from every bucket (keeps the bucket components themselves, so
	// re-populating after a regenerate doesn't repeatedly reallocate HISM components).
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	void ClearAllInstances();

	int32 NumBuckets() const { return Buckets.Num(); }

	// See ABuildingActor::SetBuildingRuntimeGrid's comment -- same World Partition RuntimeGrid
	// assignment, same caveats.
	void SetBuildingRuntimeGrid(FName GridName);

	// Batched replacement for spawning one ABuildingActor per building: builds Spec's hero meshes
	// (see FGrammarDynamicMeshBuilder, which already converts meters -> UE centimeters) and appends
	// them into this pool's single shared hero-mesh component instead of giving each building its
	// own actor + components (see AppendHeroMesh), and routes Spec's placements through AddInstance
	// exactly as ABuildingActor::ApplyBuildingSpec does -- including that same meters ->
	// UE-centimeters conversion of each placement's Location/Scale (Spec.Placements are still in
	// BuildingGrammarCore's working unit, meters; see FLocalTangentPlaneProjection's header
	// comment). ResolveKitMesh/ResolveMaterial have the same contract as ABuildingActor's -- pass
	// FGrammarKitResolver::ResolveKitMesh/ResolveMaterial (BuildingGrammarGeometry) for the real
	// implementation. Caller must call FlushHeroMeshUpdates() once after the last Spec in a
	// generation pass (not after every call -- see that method's comment).
	void ApplyBuildingSpec(
		const FGrammarBuildingSpec& Spec,
		TFunctionRef<UStaticMesh* (const FString& Role, const FString& VariantKey)> ResolveKitMesh,
		TFunctionRef<UMaterialInterface* (const FString& MaterialName, const FLinearColor& Color)> ResolveMaterial);

	// Appends BuiltMesh's triangles (already in absolute UE-centimeter world-space -- see
	// ApplyBuildingSpec's comment) into this pool's single shared hero-mesh component, lazily
	// creating it on first use. Every building's hero surfaces (walls/roofs) that share this pool
	// end up in the same DynamicMeshComponent instead of one actor+component per building; Material
	// is tracked in a small per-pool slot list and applied via the mesh's MaterialID triangle
	// attribute (UDynamicMesh enables that attribute by default). No-ops if BuiltMesh has no
	// triangles. Change notification is deferred -- see FlushHeroMeshUpdates.
	void AppendHeroMesh(const UE::Geometry::FDynamicMesh3& BuiltMesh, UMaterialInterface* Material);

	// Pushes every AppendHeroMesh() edit since the last flush to the render proxy. AppendHeroMesh
	// defers its change notification -- rebuilding the render proxy after every single building
	// would defeat the point of batching -- so callers MUST call this once after the last
	// AppendHeroMesh() in a generation pass, or the newly appended geometry never becomes visible.
	void FlushHeroMeshUpdates();

private:
	static FString MakeBucketKey(const FString& Role, const FString& VariantKey);

	UPROPERTY()
	TMap<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Buckets;

	UPROPERTY()
	TObjectPtr<UDynamicMeshComponent> HeroMeshComponent;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> HeroMaterials;
};
