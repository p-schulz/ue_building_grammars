#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TreeInstancePoolActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

// One UHierarchicalInstancedStaticMeshComponent per distinct UStaticMesh, populated by
// UTreeImportLibrary::ImportTreesFromGeoJson -- the same "one pool actor, one HISM bucket per
// distinct instanced asset" shape ABuildingInstancePoolActor already uses for building kit pieces
// (see that class's own header comment), simplified here since trees need no per-instance
// material/style bookkeeping, hero mesh, or bake-to-static-mesh support -- just many instances of a
// handful of distinct meshes.
UCLASS(BlueprintType)
class BUILDINGGRAMMARRUNTIME_API ATreeInstancePoolActor : public AActor
{
	GENERATED_BODY()

public:
	ATreeInstancePoolActor();

	// Appends one WORLD-SPACE instance transform to Mesh's bucket, creating (and assigning Mesh to)
	// the bucket's HISM component the first time this Mesh is seen. No-op if Mesh is null. Callers
	// should spawn this actor at FTransform::Identity (matching ABuildingInstancePoolActor's own
	// convention for absolute-coordinate content) -- this component-vs-actor-space distinction is
	// otherwise easy to get backwards.
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	void AddTreeInstance(UStaticMesh* Mesh, const FTransform& WorldTransform);

	int32 NumBuckets() const { return Buckets.Num(); }

	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	int32 NumInstances() const;

private:
	UPROPERTY()
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Buckets;
};
