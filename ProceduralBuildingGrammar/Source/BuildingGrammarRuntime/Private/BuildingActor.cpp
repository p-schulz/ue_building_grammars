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

void ABuildingActor::SetBuildingRuntimeGrid(FName GridName)
{
	RuntimeGrid = GridName;
}

void ABuildingActor::ApplyBuildingSpec(
	const FGrammarBuildingSpec& Spec,
	ABuildingInstancePoolActor* Pool,
	TFunctionRef<UStaticMesh* (const FString&, const FString&)> ResolveKitMesh,
	TFunctionRef<UMaterialInterface* (const FString&, const FString&, const FString&, const FLinearColor&)> ResolveMaterial)
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

		if (UMaterialInterface* Material = ResolveMaterial(MeshSpec.StyleName, MeshSpec.Role, MeshSpec.Material, MeshSpec.Color))
		{
			Component->SetMaterial(0, Material);
		}

		HeroComponents.Add(Component);
	}

	if (Pool)
	{
		// Placement.Transform's Location and Scale are in BuildingGrammarCore's working unit
		// (meters -- see FLocalTangentPlaneProjection's header comment and
		// FGrammarPlacementHelpers::MakeBoxPlacement). The shared kit unit-box mesh
		// (GrammarKitAssetBuilder.cpp) is baked spanning exactly 1 Unreal unit (1cm), so both the
		// position and the box-dimensions-as-scale must cross into centimeters here, the same
		// meters->centimeters boundary FGrammarDynamicMeshBuilder applies to hero mesh vertices.
		constexpr double MetersToUnrealUnits = 100.0;
		for (const FGrammarPlacementRecord& Placement : Spec.Placements)
		{
			// See the coordinate-convention note in BuildingActor.h -- Placement.Transform is
			// already absolute world-space (once converted to Unreal units below), so it is handed
			// to the pool as-is rather than composed with this actor's (always-identity)
			// transform. VariantKey doubles as the material name (see GrammarPlacementHelpers.h),
			// hence reusing it for both calls below.
			if (UStaticMesh* KitMesh = ResolveKitMesh(Placement.Role, Placement.VariantKey))
			{
				UMaterialInterface* Material = ResolveMaterial(Placement.StyleName, Placement.Role, Placement.VariantKey, Placement.Color);

				FTransform UnrealTransform = Placement.Transform;
				UnrealTransform.SetLocation(Placement.Transform.GetLocation() * MetersToUnrealUnits);
				UnrealTransform.SetScale3D(Placement.Transform.GetScale3D() * MetersToUnrealUnits);

				Pool->AddInstance(Placement.Role, Placement.VariantKey, UnrealTransform, KitMesh, Material, Placement.Color);
			}
		}
	}
}
