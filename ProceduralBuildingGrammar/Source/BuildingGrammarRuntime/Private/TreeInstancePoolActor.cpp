#include "TreeInstancePoolActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

ATreeInstancePoolActor::ATreeInstancePoolActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ATreeInstancePoolActor::AddTreeInstance(UStaticMesh* Mesh, const FTransform& WorldTransform)
{
	if (!Mesh)
	{
		return;
	}

	UHierarchicalInstancedStaticMeshComponent* HISM = nullptr;
	if (const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* Existing = Buckets.Find(Mesh))
	{
		HISM = *Existing;
	}
	else
	{
		HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transactional);
		HISM->SetStaticMesh(Mesh);
		HISM->SetMobility(EComponentMobility::Static);
		HISM->SetupAttachment(GetRootComponent());
		HISM->RegisterComponent();
		AddInstanceComponent(HISM);
		Buckets.Add(Mesh, HISM);
	}

	HISM->AddInstance(WorldTransform, /*bWorldSpace=*/true);
}

int32 ATreeInstancePoolActor::NumInstances() const
{
	int32 Total = 0;
	for (const TPair<TObjectPtr<UStaticMesh>, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Pair : Buckets)
	{
		if (Pair.Value)
		{
			Total += Pair.Value->GetInstanceCount();
		}
	}
	return Total;
}
