#include "BuildingInstancePoolActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMeshEditor.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Spec/BuildingSpec.h"
#include "GrammarDynamicMeshBuilder.h"
#include "GrammarKitResolver.h"
#include "GrammarKitAssetBuilder.h"
#include "Grammar/BuildingGrammarEngine.h"
#include "Engine/CollisionProfile.h"
// FMeshDescription needs to be a complete type even in non-editor stub bodies that construct one by
// value (BuildBakedMeshDescription/FinalizeBakedAsset's stubs below), so this stays outside the
// WITH_EDITOR guard unlike the rest of the editor-only asset-baking includes.
#include "MeshDescription.h"

#if WITH_EDITOR
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceConstant.h"
#include "StaticMeshAttributes.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "DynamicMeshToMeshDescription.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#endif

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

void ABuildingInstancePoolActor::AddInstance(const FString& RoleTag, const FString& VariantKey, const FTransform& Transform, UStaticMesh* Mesh, UMaterialInterface* Material, FLinearColor MaterialColor)
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
			// VariantKey doubles as the material name (see GrammarPlacementHelpers.h) -- see
			// FBuildingMaterialResolveKey's comment for why this is recorded at all.
			BucketMaterialKeys.Add(BucketKey, FBuildingMaterialResolveKey{ RoleTag, VariantKey, MaterialColor });
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
	TFunctionRef<UMaterialInterface* (const FString&, const FString&, const FLinearColor&)> ResolveMaterial)
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
		AppendHeroMesh(BuiltMesh, ResolveMaterial(MeshSpec.Role, MeshSpec.Material, MeshSpec.Color), MeshSpec.Role, MeshSpec.Material, MeshSpec.Color);
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
			UMaterialInterface* Material = ResolveMaterial(Placement.Role, Placement.VariantKey, Placement.Color);

			FTransform UnrealTransform = Placement.Transform;
			UnrealTransform.SetLocation(Placement.Transform.GetLocation() * MetersToUnrealUnits);
			UnrealTransform.SetScale3D(Placement.Transform.GetScale3D() * MetersToUnrealUnits);

			AddInstance(Placement.Role, Placement.VariantKey, UnrealTransform, KitMesh, Material, Placement.Color);
		}
	}
}

void ABuildingInstancePoolActor::AppendHeroMesh(const UE::Geometry::FDynamicMesh3& BuiltMesh, UMaterialInterface* Material, const FString& RoleTag, const FString& MaterialName, const FLinearColor& Color)
{
	if (BuiltMesh.TriangleCount() == 0)
	{
		return;
	}

	if (!HeroMeshComponent)
	{
		HeroMeshComponent = NewObject<UDynamicMeshComponent>(this, TEXT("HeroMeshes"));
		HeroMeshComponent->SetMobility(EComponentMobility::Static);
		// UDynamicMeshComponent has no collision by default (CollisionType defaults to
		// CTF_UseSimpleAsComplex with no simple collision ever set, and the component's own default
		// profile is NoCollision) -- without this, viewport line traces (the building pick tool) and
		// any other collision query would silently miss every wall/roof surface. BlockAll matches the
		// baked kit static mesh's own default body setup profile (see FinalizeBakedAsset).
		HeroMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		HeroMeshComponent->EnableComplexAsSimpleCollision();
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
			HeroMaterialKeys.Add(FBuildingMaterialResolveKey{ RoleTag, MaterialName, Color });

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

void ABuildingInstancePoolActor::SetBuildingOverride(const FString& SourceName, const FBuildingCustomizationOverride& Override)
{
	BuildingOverrides.Add(SourceName, Override);
}

void ABuildingInstancePoolActor::RegenerateFromSource()
{
	if (SourceVolumes.Num() == 0)
	{
		return;
	}

	// Tear down this cell's entire generated geometry -- mirrors ReplaceWithBakedAsset's cleanup
	// reasoning, just without destroying the actor itself. Bucket components are destroyed outright
	// (not just cleared) since a regenerated cell may no longer use the same (Role, VariantKey) set as
	// before; AddInstance lazily recreates whichever buckets the new geometry actually needs.
	for (const TPair<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& BucketPair : Buckets)
	{
		if (BucketPair.Value)
		{
			BucketPair.Value->DestroyComponent();
		}
	}
	Buckets.Reset();
	BucketMaterialKeys.Reset();

	if (HeroMeshComponent)
	{
		HeroMeshComponent->DestroyComponent();
		HeroMeshComponent = nullptr;
	}
	HeroMaterials.Reset();
	HeroMaterialKeys.Reset();

	for (const FGrammarBuildingVolume& Volume : SourceVolumes)
	{
		// Apply this building's override (if any): TagOverrides win over the building's own VolumeTags
		// on conflict, and a non-empty ForcedStyleName narrows the config's Styles down to just that
		// one entry -- both feed GenerateBuildingSpec's ordinary (Tags, Config) inputs unchanged, so
		// its existing tag-based style-selection logic does the rest without any special-casing here.
		const FBuildingCustomizationOverride* Override = BuildingOverrides.Find(Volume.SourceName);

		TMap<FString, FString> EffectiveTags = Volume.VolumeTags;
		FBuildingGrammarConfig NarrowedConfig;
		const FBuildingGrammarConfig* EffectiveConfig = &SourceConfig;

		if (Override)
		{
			for (const TPair<FString, FString>& OverridePair : Override->TagOverrides)
			{
				EffectiveTags.Add(OverridePair.Key, OverridePair.Value);
			}

			if (!Override->ForcedStyleName.IsEmpty())
			{
				NarrowedConfig = SourceConfig;
				NarrowedConfig.Styles.RemoveAll([Override](const FFacadeStyleConfig& Style)
				{
					return Style.Name != Override->ForcedStyleName;
				});
				if (NarrowedConfig.Styles.Num() > 0)
				{
					EffectiveConfig = &NarrowedConfig;
				}
			}
		}

		FGrammarBuildingSpec Spec;
		FString GenerationError;
		if (!FBuildingGrammarEngine::GenerateBuildingSpec(Volume.Footprint.OuterRing, EffectiveTags, *EffectiveConfig, Volume.SourceName, Spec, GenerationError))
		{
			continue;
		}
		FBuildingGrammarEngine::ApplyMinHeightOffset(Spec, Volume.MinHeight);

		ApplyBuildingSpec(Spec, &FGrammarKitResolver::ResolveKitMesh, &FGrammarKitResolver::ResolveMaterial);
	}

	FlushHeroMeshUpdates();
}

void ABuildingInstancePoolActor::PostLoad()
{
	Super::PostLoad();

	// See FBuildingMaterialResolveKey's comment: every material reference on this actor's
	// components is a runtime UMaterialInstanceDynamic that does not survive being saved as part of
	// this actor's own external package (Outer=GetTransientPackage(), RF_Transient -- see
	// FGrammarKitResolver::ResolveMaterial) -- comes back null after a reload. Re-resolve (cheap:
	// ResolveMaterial caches by (Role, MaterialName) and only recreates the MID if the cache entry
	// itself didn't survive either) and reassign every one from the plain FString/FLinearColor data
	// that DOES survive.
	for (const TPair<FString, FBuildingMaterialResolveKey>& Entry : BucketMaterialKeys)
	{
		TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* Bucket = Buckets.Find(Entry.Key);
		if (!Bucket || !Bucket->Get())
		{
			continue;
		}
		const FBuildingMaterialResolveKey& Key = Entry.Value;
		if (UMaterialInterface* Material = FGrammarKitResolver::ResolveMaterial(Key.Role, Key.MaterialName, Key.Color))
		{
			Bucket->Get()->SetMaterial(0, Material);
		}
	}

	if (HeroMeshComponent && HeroMaterialKeys.Num() > 0)
	{
		TArray<UMaterialInterface*> RawMaterials;
		RawMaterials.Reserve(HeroMaterialKeys.Num());
		for (int32 Index = 0; Index < HeroMaterialKeys.Num(); ++Index)
		{
			const FBuildingMaterialResolveKey& Key = HeroMaterialKeys[Index];
			UMaterialInterface* Material = FGrammarKitResolver::ResolveMaterial(Key.Role, Key.MaterialName, Key.Color);
			if (HeroMaterials.IsValidIndex(Index))
			{
				HeroMaterials[Index] = Material;
			}
			RawMaterials.Add(Material);
		}
		HeroMeshComponent->ConfigureMaterialSet(RawMaterials);
	}
}

#if WITH_EDITOR

namespace
{
	UPackage* CreateBakedAssetPackage(const FString& PackagePath)
	{
		UPackage* Package = CreatePackage(*PackagePath);
		Package->FullyLoad();
		return Package;
	}

	bool SaveBakedAssetPackage(UPackage* Package, UObject* Asset, const FString& PackagePath)
	{
		Asset->PostEditChange();
		FAssetRegistryModule::AssetCreated(Asset);
		Package->MarkPackageDirty();

		const FString FileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
	}
}

FBuildingBakeExtractedData ABuildingInstancePoolActor::ExtractBakeData() const
{
	using namespace UE::Geometry;

	FBuildingBakeExtractedData Data;

	// Converted once per editor session (this static persists across every pool actor this is ever
	// called on), not once per cell -- the shared kit box mesh never changes mid-session.
	static FDynamicMesh3 CachedKitMesh;
	static bool bCachedKitMeshValid = false;
	if (!bCachedKitMeshValid)
	{
		UStaticMesh* KitMesh = nullptr;
		for (const TPair<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& BucketPair : Buckets)
		{
			if (BucketPair.Value && BucketPair.Value->GetStaticMesh())
			{
				KitMesh = BucketPair.Value->GetStaticMesh();
				break;
			}
		}
		if (KitMesh)
		{
			if (FMeshDescription* KitMeshDescription = KitMesh->GetMeshDescription(0))
			{
				FMeshDescriptionToDynamicMesh Converter;
				Converter.Convert(KitMeshDescription, CachedKitMesh);
				bCachedKitMeshValid = true;
			}
		}
	}
	Data.KitMesh = CachedKitMesh;

	// Dense 0..N-1 slot indices, one per distinct persistent material actually used -- matches
	// FDynamicMeshToMeshDescription::Convert's own behavior of creating one polygon group per
	// distinct MaterialID value found (0..max), so this array's Num() lines up with the polygon
	// group / static-material-slot count later without any gaps.
	auto FindOrAddMaterialSlot = [&Data](UMaterialInterface* Material) -> int32
	{
		if (!Material)
		{
			return 0;
		}
		const int32 Existing = Data.BakedMaterials.IndexOfByKey(Material);
		return Existing != INDEX_NONE ? Existing : Data.BakedMaterials.Add(Material);
	};

	for (const TPair<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& BucketPair : Buckets)
	{
		UHierarchicalInstancedStaticMeshComponent* Bucket = BucketPair.Value;
		if (!Bucket || Bucket->GetInstanceCount() == 0)
		{
			continue;
		}

		UMaterialInterface* BakedMaterial = nullptr;
		if (const FBuildingMaterialResolveKey* Key = BucketMaterialKeys.Find(BucketPair.Key))
		{
			BakedMaterial = FGrammarKitAssetBuilder::GetOrCreateColorVariant(Key->Role, Key->MaterialName, Key->Color);
		}

		FBuildingBakeExtractedData::FBucketInstances BucketInstances;
		BucketInstances.MaterialSlot = FindOrAddMaterialSlot(BakedMaterial);

		const int32 InstanceCount = Bucket->GetInstanceCount();
		BucketInstances.InstanceTransforms.Reserve(InstanceCount);
		for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
		{
			FTransform InstanceTransform;
			if (Bucket->GetInstanceTransform(InstanceIndex, InstanceTransform, /*bWorldSpace=*/true))
			{
				BucketInstances.InstanceTransforms.Add(InstanceTransform);
			}
		}
		Data.Buckets.Add(MoveTemp(BucketInstances));
	}

	if (HeroMeshComponent && HeroMaterialKeys.Num() > 0)
	{
		Data.HeroSlotToBakedSlot.Reserve(HeroMaterialKeys.Num());
		for (const FBuildingMaterialResolveKey& Key : HeroMaterialKeys)
		{
			UMaterialInterface* BakedMaterial = FGrammarKitAssetBuilder::GetOrCreateColorVariant(Key.Role, Key.MaterialName, Key.Color);
			Data.HeroSlotToBakedSlot.Add(FindOrAddMaterialSlot(BakedMaterial));
		}

		// Moved out, not copied -- see this method's declaration comment. Left in a fresh, empty,
		// well-defined state rather than relying on FDynamicMesh3's unspecified moved-from state,
		// since nothing here guarantees what that looks like.
		HeroMeshComponent->GetDynamicMesh()->EditMesh([&Data](FDynamicMesh3& PoolMesh)
		{
			Data.HeroMesh = MoveTemp(PoolMesh);
			PoolMesh = FDynamicMesh3();
		}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, /*bDeferChangeEvents=*/true);
	}

	return Data;
}

FMeshDescription ABuildingInstancePoolActor::BuildBakedMeshDescription(const FBuildingBakeExtractedData& Data)
{
	using namespace UE::Geometry;

	FDynamicMesh3 CombinedMesh;
	CombinedMesh.EnableAttributes();
	CombinedMesh.Attributes()->EnableMaterialID();
	FDynamicMeshMaterialAttribute* CombinedMaterialIDs = CombinedMesh.Attributes()->GetMaterialID();
	FDynamicMeshEditor CombinedEditor(&CombinedMesh);

	// Kit box instances: baking is the one place this pool's instanced geometry has to become real,
	// unique triangles rather than GPU instances.
	for (const FBuildingBakeExtractedData::FBucketInstances& BucketInstances : Data.Buckets)
	{
		for (const FTransform& InstanceTransform : BucketInstances.InstanceTransforms)
		{
			// Normals need the inverse-scale (not the forward scale used for positions) so they stay
			// correctly perpendicular under this box's generally non-uniform per-instance scale
			// (Width/Depth/Height -- see AddInstance's callers) -- the standard inverse-transpose
			// simplification for a pure rotation*diagonal-scale transform.
			const FQuat Rotation = InstanceTransform.GetRotation();
			const FVector Scale = InstanceTransform.GetScale3D();
			const FVector InvScale(
				FMath::IsNearlyZero(Scale.X) ? 0.0 : 1.0 / Scale.X,
				FMath::IsNearlyZero(Scale.Y) ? 0.0 : 1.0 / Scale.Y,
				FMath::IsNearlyZero(Scale.Z) ? 0.0 : 1.0 / Scale.Z);

			FMeshIndexMappings IndexMaps;
			CombinedEditor.AppendMesh(&Data.KitMesh, IndexMaps,
				[&InstanceTransform](int32, const FVector3d& Position) -> FVector3d
				{
					return InstanceTransform.TransformPosition(FVector(Position));
				},
				[Rotation, InvScale](int32, const FVector3d& Normal) -> FVector3d
				{
					const FVector Scaled(Normal.X * InvScale.X, Normal.Y * InvScale.Y, Normal.Z * InvScale.Z);
					return Rotation.RotateVector(Scaled).GetSafeNormal();
				});

			for (const TPair<int32, int32>& TrianglePair : IndexMaps.GetTriangleMap().GetForwardMap())
			{
				CombinedMaterialIDs->SetValue(TrianglePair.Value, BucketInstances.MaterialSlot);
			}
		}
	}

	// Hero mesh: remap its existing per-triangle MaterialIDs onto persistent baked material slots via
	// HeroSlotToBakedSlot, then append as-is -- already absolute world-space, same as every kit
	// instance above, so no per-vertex transform is needed here.
	if (Data.HeroMesh.TriangleCount() > 0)
	{
		const FDynamicMeshMaterialAttribute* HeroMaterialIDs = (Data.HeroMesh.HasAttributes() && Data.HeroMesh.Attributes()->HasMaterialID())
			? Data.HeroMesh.Attributes()->GetMaterialID() : nullptr;

		FMeshIndexMappings IndexMaps;
		CombinedEditor.AppendMesh(&Data.HeroMesh, IndexMaps);

		for (const TPair<int32, int32>& TrianglePair : IndexMaps.GetTriangleMap().GetForwardMap())
		{
			const int32 OldSlot = HeroMaterialIDs ? HeroMaterialIDs->GetValue(TrianglePair.Key) : 0;
			const int32 NewSlot = Data.HeroSlotToBakedSlot.IsValidIndex(OldSlot) ? Data.HeroSlotToBakedSlot[OldSlot] : 0;
			CombinedMaterialIDs->SetValue(TrianglePair.Value, NewSlot);
		}
	}

	FMeshDescription MeshDescription;
	if (CombinedMesh.TriangleCount() == 0)
	{
		return MeshDescription; // empty -- FinalizeBakedAsset checks for this and returns nullptr
	}

	// Deliberately NOT recentered around a local pivot -- see BakeToStaticMesh's header comment for
	// why the asset needs to keep its original absolute world-space coordinates baked directly into
	// the geometry instead.
	FStaticMeshAttributes Attributes(MeshDescription);
	Attributes.Register();

	FDynamicMeshToMeshDescription DescriptionConverter;
	DescriptionConverter.Convert(&CombinedMesh, MeshDescription);
	return MeshDescription;
}

UStaticMesh* ABuildingInstancePoolActor::FinalizeBakedAsset(FMeshDescription&& MeshDescription, const TArray<TObjectPtr<UMaterialInterface>>& BakedMaterials, const FString& PackagePath)
{
	if (MeshDescription.Polygons().Num() == 0)
	{
		return nullptr;
	}

	UPackage* Package = CreateBakedAssetPackage(PackagePath);
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, FName(*FPackageName::GetShortName(PackagePath)), RF_Public | RF_Standalone);
	const int32 SlotCount = FMath::Max(BakedMaterials.Num(), 1);
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		FStaticMaterial StaticMaterial;
		if (BakedMaterials.IsValidIndex(SlotIndex))
		{
			StaticMaterial.MaterialInterface = BakedMaterials[SlotIndex];
		}
		StaticMesh->GetStaticMaterials().Add(StaticMaterial);
	}
	StaticMesh->GetNaniteSettings().bEnabled = true;

	FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();
	SourceModel.BuildSettings.bRecomputeNormals = false;
	SourceModel.BuildSettings.bRecomputeTangents = true;
	SourceModel.BuildSettings.bGenerateLightmapUVs = true;

	TArray<const FMeshDescription*> MeshDescriptionPtrs;
	MeshDescriptionPtrs.Add(&MeshDescription);
	StaticMesh->BuildFromMeshDescriptions(MeshDescriptionPtrs);

	SaveBakedAssetPackage(Package, StaticMesh, PackagePath);
	return StaticMesh;
}

UStaticMesh* ABuildingInstancePoolActor::BakeToStaticMesh(const FString& PackagePath) const
{
	FBuildingBakeExtractedData Data = ExtractBakeData();
	FMeshDescription MeshDescription = BuildBakedMeshDescription(Data);
	return FinalizeBakedAsset(MoveTemp(MeshDescription), Data.BakedMaterials, PackagePath);
}

AStaticMeshActor* ABuildingInstancePoolActor::ReplaceWithBakedAsset(ABuildingInstancePoolActor*& PoolActor, UStaticMesh* BakedMesh)
{
	if (!PoolActor || !BakedMesh)
	{
		return nullptr;
	}

	UWorld* World = PoolActor->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Identity transform: the baked geometry already carries this pool's original absolute
	// world-space coordinates directly (see BakeToStaticMesh's header comment for why), so the new
	// actor doesn't need to (and must not) apply any additional offset of its own.
	AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!NewActor)
	{
		return nullptr;
	}
	NewActor->SetActorLabel(PoolActor->GetActorLabel());
	NewActor->SetRuntimeGrid(PoolActor->GetRuntimeGrid());
	if (UStaticMeshComponent* MeshComponent = NewActor->GetStaticMeshComponent())
	{
		MeshComponent->SetStaticMesh(BakedMesh);
		MeshComponent->SetMobility(EComponentMobility::Static);
	}

	// Safe to destroy outright, unlike FBuildingActorPersistence's save-then-unload case -- see this
	// method's declaration comment for why. Nulling the caller's pointer here (not just locally) is
	// deliberate: PoolActor is dangling the instant this call returns, so any caller still holding
	// the pointer needs to be forced to notice rather than risk a use-after-free.
	World->DestroyActor(PoolActor);
	PoolActor = nullptr;

	return NewActor;
}

AStaticMeshActor* ABuildingInstancePoolActor::BakeAndReplace(ABuildingInstancePoolActor*& PoolActor, const FString& PackagePath)
{
	if (!PoolActor)
	{
		return nullptr;
	}

	UStaticMesh* BakedMesh = PoolActor->BakeToStaticMesh(PackagePath);
	if (!BakedMesh)
	{
		return nullptr;
	}

	return ReplaceWithBakedAsset(PoolActor, BakedMesh);
}

FString ABuildingInstancePoolActor::MakeDefaultBakedAssetPath() const
{
	const UWorld* World = GetWorld();
	const FString LevelName = World ? FPackageName::GetShortName(World->GetOutermost()->GetName()) : TEXT("UnknownLevel");

	FString SanitizedLabel;
	for (const TCHAR Ch : GetActorLabel())
	{
		SanitizedLabel.AppendChar(FChar::IsAlnum(Ch) ? Ch : TEXT('_'));
	}
	if (SanitizedLabel.IsEmpty())
	{
		SanitizedLabel = TEXT("Pool");
	}

	return FString::Printf(TEXT("/Game/GeneratedBuildings/%s/SM_%s"), *LevelName, *SanitizedLabel);
}

#else // !WITH_EDITOR

FBuildingBakeExtractedData ABuildingInstancePoolActor::ExtractBakeData() const
{
	return FBuildingBakeExtractedData();
}

FMeshDescription ABuildingInstancePoolActor::BuildBakedMeshDescription(const FBuildingBakeExtractedData&)
{
	return FMeshDescription();
}

UStaticMesh* ABuildingInstancePoolActor::FinalizeBakedAsset(FMeshDescription&&, const TArray<TObjectPtr<UMaterialInterface>>&, const FString&)
{
	return nullptr;
}

UStaticMesh* ABuildingInstancePoolActor::BakeToStaticMesh(const FString&) const
{
	return nullptr;
}

AStaticMeshActor* ABuildingInstancePoolActor::ReplaceWithBakedAsset(ABuildingInstancePoolActor*&, UStaticMesh*)
{
	return nullptr;
}

AStaticMeshActor* ABuildingInstancePoolActor::BakeAndReplace(ABuildingInstancePoolActor*&, const FString&)
{
	return nullptr;
}

FString ABuildingInstancePoolActor::MakeDefaultBakedAssetPath() const
{
	return FString();
}

#endif // WITH_EDITOR
