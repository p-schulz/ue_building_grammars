#include "BuildingInstancePoolActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMeshEditor.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Spec/BuildingSpec.h"
#include "GrammarDynamicMeshBuilder.h"

ABuildingInstancePoolActor::ABuildingInstancePoolActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// Every HISM bucket and the shared hero-mesh component are marked Static before attaching to
	// this Root (AddInstance/AppendHeroMesh) -- a Static child can't attach to a non-Static parent
	// (the engine aborts the attach and logs a warning), and Root defaulted to Movable. This actor
	// is generated once and never moved (see AddInstance/AppendHeroMesh's absolute-world-space
	// coordinate convention), so Static is also the semantically correct mobility here, not just a
	// warning workaround.
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ABuildingInstancePoolActor::SetBuildingRuntimeGrid(FName GridName)
{
	RuntimeGrid = GridName;
}

FString ABuildingInstancePoolActor::MakeBucketKey(const FString& Role, const FString& VariantKey)
{
	return Role + TEXT("|") + VariantKey;
}

void ABuildingInstancePoolActor::AddInstance(const FString& RoleTag, const FString& VariantKey, const FTransform& Transform, UStaticMesh* Mesh, UMaterialInterface* Material)
{
	if (!Mesh)
	{
		return;
	}

	const FString BucketKey = MakeBucketKey(RoleTag, VariantKey);
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* ExistingBucket = Buckets.Find(BucketKey);
	UHierarchicalInstancedStaticMeshComponent* Bucket = ExistingBucket ? ExistingBucket->Get() : nullptr;

	if (!Bucket)
	{
		Bucket = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), *BucketKey));
		Bucket->SetStaticMesh(Mesh);
		if (Material)
		{
			Bucket->SetMaterial(0, Material);
		}
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

void ABuildingInstancePoolActor::ApplyBuildingSpec(
	const FGrammarBuildingSpec& Spec,
	TFunctionRef<UStaticMesh* (const FString&, const FString&)> ResolveKitMesh,
	TFunctionRef<UMaterialInterface* (const FString&, const FLinearColor&)> ResolveMaterial)
{
	for (const FGrammarMeshSpec& MeshSpec : Spec.HeroMeshes)
	{
		if (MeshSpec.Vertices.Num() == 0 || MeshSpec.Faces.Num() == 0)
		{
			continue;
		}

		// FGrammarDynamicMeshBuilder already converts MeshSpec.Vertices (meters, BuildingGrammarCore's
		// working unit) into UE centimeters -- see its own header/implementation comment.
		UE::Geometry::FDynamicMesh3 BuiltMesh;
		FGrammarDynamicMeshBuilder::BuildDynamicMesh(MeshSpec, BuiltMesh);
		AppendHeroMesh(BuiltMesh, ResolveMaterial(MeshSpec.Material, MeshSpec.Color));
	}

	// Placement.Transform's Location and Scale are still in BuildingGrammarCore's working unit
	// (meters -- see FLocalTangentPlaneProjection's header comment and
	// FGrammarPlacementHelpers::MakeBoxPlacement). The shared kit unit-box mesh
	// (GrammarKitAssetBuilder.cpp) is baked spanning exactly 1 Unreal unit (1cm), so both the
	// position and the box-dimensions-as-scale must cross into centimeters here, the same
	// meters->centimeters boundary FGrammarDynamicMeshBuilder applies to hero mesh vertices.
	constexpr double MetersToUnrealUnits = 100.0;
	for (const FGrammarPlacementRecord& Placement : Spec.Placements)
	{
		// VariantKey doubles as the material name (see GrammarPlacementHelpers.h), hence reusing
		// it for both calls below.
		if (UStaticMesh* KitMesh = ResolveKitMesh(Placement.Role, Placement.VariantKey))
		{
			UMaterialInterface* Material = ResolveMaterial(Placement.VariantKey, Placement.Color);

			FTransform UnrealTransform = Placement.Transform;
			UnrealTransform.SetLocation(Placement.Transform.GetLocation() * MetersToUnrealUnits);
			UnrealTransform.SetScale3D(Placement.Transform.GetScale3D() * MetersToUnrealUnits);

			AddInstance(Placement.Role, Placement.VariantKey, UnrealTransform, KitMesh, Material);
		}
	}
}

void ABuildingInstancePoolActor::AppendHeroMesh(const UE::Geometry::FDynamicMesh3& BuiltMesh, UMaterialInterface* Material)
{
	if (BuiltMesh.TriangleCount() == 0)
	{
		return;
	}

	if (!HeroMeshComponent)
	{
		HeroMeshComponent = NewObject<UDynamicMeshComponent>(this, TEXT("HeroMeshes"));
		HeroMeshComponent->SetMobility(EComponentMobility::Static);
		HeroMeshComponent->SetupAttachment(GetRootComponent());
		HeroMeshComponent->RegisterComponent();
	}

	int32 MaterialSlot = 0;
	if (Material)
	{
		MaterialSlot = HeroMaterials.IndexOfByKey(Material);
		if (MaterialSlot == INDEX_NONE)
		{
			MaterialSlot = HeroMaterials.Add(Material);

			TArray<UMaterialInterface*> RawMaterials;
			RawMaterials.Reserve(HeroMaterials.Num());
			for (const TObjectPtr<UMaterialInterface>& HeroMaterial : HeroMaterials)
			{
				RawMaterials.Add(HeroMaterial.Get());
			}
			HeroMeshComponent->ConfigureMaterialSet(RawMaterials);
		}
	}

	HeroMeshComponent->GetDynamicMesh()->EditMesh([&BuiltMesh, MaterialSlot](UE::Geometry::FDynamicMesh3& PoolMesh)
	{
		UE::Geometry::FDynamicMeshEditor Editor(&PoolMesh);
		UE::Geometry::FMeshIndexMappings IndexMaps;
		Editor.AppendMesh(&BuiltMesh, IndexMaps);

		if (!PoolMesh.Attributes()->HasMaterialID())
		{
			PoolMesh.Attributes()->EnableMaterialID();
		}
		UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIDs = PoolMesh.Attributes()->GetMaterialID();
		for (const TPair<int32, int32>& TrianglePair : IndexMaps.GetTriangleMap().GetForwardMap())
		{
			MaterialIDs->SetValue(TrianglePair.Value, MaterialSlot);
		}
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, /*bDeferChangeEvents=*/true);
}

void ABuildingInstancePoolActor::FlushHeroMeshUpdates()
{
	if (HeroMeshComponent)
	{
		HeroMeshComponent->NotifyMeshUpdated();
	}
}
