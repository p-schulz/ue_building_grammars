#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingInstancePoolActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

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

private:
	static FString MakeBucketKey(const FString& Role, const FString& VariantKey);

	UPROPERTY()
	TMap<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Buckets;
};
