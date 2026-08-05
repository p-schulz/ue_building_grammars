#include "BuildingActor.h"
#include "BuildingInstancePoolActor.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "Spec/BuildingSpec.h"
#include "GrammarDynamicMeshBuilder.h"

ABuildingActor::ABuildingActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABuildingActor::ApplyBuildingSpec(
	const FGrammarBuildingSpec& Spec,
	ABuildingInstancePoolActor* Pool,
	TFunctionRef<UStaticMesh* (const FString&, const FString&)> ResolveKitMesh,
	TFunctionRef<UMaterialInterface* (const FString&, const FLinearColor&)> ResolveMaterial)
{
	for (UDynamicMeshComponent* Existing : HeroComponents)
	{
		if (Existing)
		{
			Existing->DestroyComponent();
		}
	}
	HeroComponents.Reset();

	for (int32 Index = 0; Index < Spec.HeroMeshes.Num(); ++Index)
	{
		const FGrammarMeshSpec& MeshSpec = Spec.HeroMeshes[Index];
		if (MeshSpec.Vertices.Num() == 0 || MeshSpec.Faces.Num() == 0)
		{
			continue;
		}

		UE::Geometry::FDynamicMesh3 BuiltMesh;
		FGrammarDynamicMeshBuilder::BuildDynamicMesh(MeshSpec, BuiltMesh);

		UDynamicMeshComponent* Component = NewObject<UDynamicMeshComponent>(this, MakeUniqueObjectName(this, UDynamicMeshComponent::StaticClass(), *FString::Printf(TEXT("Hero_%d"), Index)));
		Component->SetupAttachment(GetRootComponent());
		Component->RegisterComponent();
		Component->GetDynamicMesh()->SetMesh(MoveTemp(BuiltMesh));

		if (UMaterialInterface* Material = ResolveMaterial(MeshSpec.Material, MeshSpec.Color))
		{
			Component->SetMaterial(0, Material);
		}

		HeroComponents.Add(Component);
	}

	if (Pool)
	{
		for (const FGrammarPlacementRecord& Placement : Spec.Placements)
		{
			// See the coordinate-convention note in BuildingActor.h -- Placement.Transform is
			// already absolute world-space, so it is handed to the pool as-is rather than composed
			// with this actor's (always-identity) transform. VariantKey doubles as the material
			// name (see GrammarPlacementHelpers.h), hence reusing it for both calls below.
			if (UStaticMesh* KitMesh = ResolveKitMesh(Placement.Role, Placement.VariantKey))
			{
				UMaterialInterface* Material = ResolveMaterial(Placement.VariantKey, Placement.Color);
				Pool->AddInstance(Placement.Role, Placement.VariantKey, Placement.Transform, KitMesh, Material);
			}
		}
	}
}
