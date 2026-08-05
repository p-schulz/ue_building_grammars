#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingInstancePoolActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

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
	// HISM component (and assigning it Mesh) the first time that (Role, VariantKey) pair is seen.
	// Mesh must be the same UStaticMesh every time for a given (Role, VariantKey) pair -- it is
	// only actually consulted on first use; a mismatched Mesh on a later call is silently ignored
	// (matches HISM's own single-static-mesh-per-component constraint). No-ops if Mesh is null,
	// since there is no baked kit mesh yet to resolve VariantKey to -- see docs/PLAN.md section 4.
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	void AddInstance(const FString& Role, const FString& VariantKey, const FTransform& Transform, UStaticMesh* Mesh);

	// Removes every instance from every bucket (keeps the bucket components themselves, so
	// re-populating after a regenerate doesn't repeatedly reallocate HISM components).
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	void ClearAllInstances();

	int32 NumBuckets() const { return Buckets.Num(); }

private:
	static FString MakeBucketKey(const FString& Role, const FString& VariantKey);

	UPROPERTY()
	TMap<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Buckets;
};
