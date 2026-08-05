#include "BuildingInstancePoolActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

ABuildingInstancePoolActor::ABuildingInstancePoolActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

FString ABuildingInstancePoolActor::MakeBucketKey(const FString& Role, const FString& VariantKey)
{
	return Role + TEXT("|") + VariantKey;
}

void ABuildingInstancePoolActor::AddInstance(const FString& Role, const FString& VariantKey, const FTransform& Transform, UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return;
	}

	const FString BucketKey = MakeBucketKey(Role, VariantKey);
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* ExistingBucket = Buckets.Find(BucketKey);
	UHierarchicalInstancedStaticMeshComponent* Bucket = ExistingBucket ? ExistingBucket->Get() : nullptr;

	if (!Bucket)
	{
		Bucket = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), *BucketKey));
		Bucket->SetStaticMesh(Mesh);
		Bucket->SetMobility(EComponentMobility::Static);
		Bucket->SetupAttachment(GetRootComponent());
		Bucket->RegisterComponent();
		Buckets.Add(BucketKey, Bucket);
	}

	Bucket->AddInstance(Transform, /*bWorldSpace=*/true);
}

void ABuildingInstancePoolActor::ClearAllInstances()
{
	for (const TPair<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& BucketPair : Buckets)
	{
		if (UHierarchicalInstancedStaticMeshComponent* Bucket = BucketPair.Value)
		{
			Bucket->ClearInstances();
		}
	}
}
