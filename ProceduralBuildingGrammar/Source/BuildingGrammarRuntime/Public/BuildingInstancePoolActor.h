#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Osm/BuildingPartResolver.h"
#include "Config/BuildingGrammarConfig.h"
#include "BuildingInstancePoolActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UDynamicMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class AStaticMeshActor;
struct FGrammarBuildingSpec;
struct FMeshDescription;

// Records what FGrammarKitResolver::ResolveMaterial was called with for a given bucket/hero-material
// slot, so the actual UMaterialInterface* (always a UMaterialInstanceDynamic -- see ResolveMaterial's
// comment) can be re-resolved after a reload instead of persisted directly. MIDs are created with
// Outer=GetTransientPackage() and are RF_Transient (GrammarKitResolver.cpp), so a hard UPROPERTY
// reference to one comes back null after this actor's external package is saved and reloaded -- see
// PostLoad.
USTRUCT()
struct FBuildingMaterialResolveKey
{
	GENERATED_BODY()

	UPROPERTY()
	FString Role;

	UPROPERTY()
	FString MaterialName;

	UPROPERTY()
	FLinearColor Color = FLinearColor::White;
};

// Per-building post-import customization, keyed by FGrammarBuildingVolume::SourceName in
// ABuildingInstancePoolActor::BuildingOverrides. Applied on top of that building's own resolved
// FGrammarBuildingVolume::VolumeTags/config by RegenerateFromSource -- see that method's comment.
// Both fields are pure-data overrides that flow straight into FBuildingGrammarEngine::GenerateBuildingSpec's
// existing (Tags, Config) inputs, so no changes to BuildingGrammarCore's generation logic are needed.
USTRUCT()
struct FBuildingCustomizationOverride
{
	GENERATED_BODY()

	// Merged onto the building's own VolumeTags before generation (these entries win on conflict).
	UPROPERTY(EditAnywhere)
	TMap<FString, FString> TagOverrides;

	// If non-empty, generation uses a config copy whose Styles array contains only the
	// FFacadeStyleConfig with this Name, instead of this building's normal tag-based style selection.
	// Empty means "no override, use tag-based selection as usual".
	UPROPERTY(EditAnywhere)
	FString ForcedStyleName;
};

// Plain data extracted from a pool actor's live components (see
// ABuildingInstancePoolActor::ExtractBakeData), ready to hand to a background task. Not a USTRUCT
// (not reflected/serialized, and FDynamicMesh3 isn't UPROPERTY-compatible anyway) -- holds no
// UObject references that need thread-safe access except BakedMaterials, which is only ever
// dereferenced back on the game thread (see BuildBakedMeshDescription's comment for why that's
// safe to carry across the thread boundary unused).
struct FBuildingBakeExtractedData
{
	struct FBucketInstances
	{
		int32 MaterialSlot = 0;
		TArray<FTransform> InstanceTransforms;
	};

	// Shared kit box mesh, converted to FDynamicMesh3 once per editor session (see ExtractBakeData),
	// not reconverted per cell.
	UE::Geometry::FDynamicMesh3 KitMesh;
	TArray<FBucketInstances> Buckets;

	// Moved (not copied) out of the hero mesh component by ExtractBakeData -- cheap, and safe
	// because the pool actor this came from is going to be destroyed once baking finishes either
	// way. Its own MaterialID attribute values are hero-local indices into HeroSlotToBakedSlot, not
	// directly into BakedMaterials.
	UE::Geometry::FDynamicMesh3 HeroMesh;
	TArray<int32> HeroSlotToBakedSlot;

	// Dense 0..N-1 persistent materials, indexed by Buckets[i].MaterialSlot and
	// HeroSlotToBakedSlot's mapped values.
	TArray<TObjectPtr<UMaterialInterface>> BakedMaterials;
};

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
	// its mesh's default material). MaterialColor is only used to remember how to re-resolve
	// Material after a reload (see PostLoad) -- it plays no role in rendering directly, since
	// Material itself (if not null) is expected to already have it baked in (see
	// FGrammarKitResolver::ResolveMaterial). Pass FLinearColor::White if Material is null or the
	// caller doesn't care about this bucket surviving a save+reload cycle.
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	void AddInstance(const FString& RoleTag, const FString& VariantKey, const FTransform& Transform, UStaticMesh* Mesh, UMaterialInterface* Material = nullptr, FLinearColor MaterialColor = FLinearColor::White);

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
		TFunctionRef<UMaterialInterface* (const FString& Role, const FString& MaterialName, const FLinearColor& Color)> ResolveMaterial);

	// Appends BuiltMesh's triangles (already in absolute UE-centimeter world-space -- see
	// ApplyBuildingSpec's comment) into this pool's single shared hero-mesh component, lazily
	// creating it on first use. Every building's hero surfaces (walls/roofs) that share this pool
	// end up in the same DynamicMeshComponent instead of one actor+component per building; Material
	// is tracked in a small per-pool slot list and applied via the mesh's MaterialID triangle
	// attribute (UDynamicMesh enables that attribute by default). No-ops if BuiltMesh has no
	// triangles. Change notification is deferred -- see FlushHeroMeshUpdates. Role/MaterialName/
	// Color record how Material was resolved, purely for PostLoad re-resolution (see AddInstance's
	// MaterialColor comment) -- pass whatever ResolveMaterial was called with, or empty strings if
	// Material is null.
	void AppendHeroMesh(const UE::Geometry::FDynamicMesh3& BuiltMesh, UMaterialInterface* Material, const FString& RoleTag, const FString& MaterialName, const FLinearColor& Color);

	// Pushes every AppendHeroMesh() edit since the last flush to the render proxy. AppendHeroMesh
	// defers its change notification -- rebuilding the render proxy after every single building
	// would defeat the point of batching -- so callers MUST call this once after the last
	// AppendHeroMesh() in a generation pass, or the newly appended geometry never becomes visible.
	void FlushHeroMeshUpdates();

	// Every material this actor's components reference is a runtime UMaterialInstanceDynamic (see
	// FBuildingMaterialResolveKey's comment) that cannot survive this actor's own external-package
	// save/reload cycle as a plain UPROPERTY reference -- re-resolves and reassigns them all from
	// BucketMaterialKeys/HeroMaterialKeys, which DO survive (plain FString/FLinearColor data).
	virtual void PostLoad() override;

	// Editor-only. Bakes every HISM bucket instance and the hero mesh into one combined UStaticMesh,
	// saved as a new asset under PackagePath. Geometry keeps this actor's original absolute
	// world-space coordinates unmodified (deliberately NOT recentered around a local pivot): the
	// asset must reproduce its correct position on its own when placed at identity transform in any
	// level (including a fresh one with no memory of this actor's original transform) -- e.g.
	// dragging several baked cells into a new/empty level must NOT collapse them all on top of each
	// other at that level's origin, which is exactly what recentering each one around its own bounds
	// would do. Persistent per-color material variants (FGrammarKitAssetBuilder::GetOrCreateColorVariant)
	// are used in the baked mesh's material slots instead of this actor's transient runtime MIDs, so
	// they survive normally as part of the saved asset like any other UStaticMesh. Returns nullptr
	// (no asset created) if this pool has no geometry to bake.
	//
	// This is a synchronous, all-on-the-calling-thread composition of ExtractBakeData/
	// BuildBakedMeshDescription/FinalizeBakedAsset below, for callers (BakeAndReplace, i.e. the
	// standalone "Bake Generated Buildings to Static Meshes..." menu action) that don't need to
	// overlap baking with other work. UBuildingGenerationLibrary::GenerateBuildingsFromOsmFileChunked's
	// bBakeToStaticMeshPerCell path calls the three phases directly instead, so a cell's expensive,
	// UObject-free merge work (BuildBakedMeshDescription) can run on a background task while the next
	// cell generates -- see that function's own comment for why.
	UStaticMesh* BakeToStaticMesh(const FString& PackagePath) const;

	// Phase 1/3 (game thread only -- may create/save persistent color-variant material assets via
	// FGrammarKitAssetBuilder::GetOrCreateColorVariant): copies/moves this pool's live component data
	// into a plain snapshot safe to hand to a background task. The hero mesh's geometry is MOVED (not
	// copied) out of HeroMeshComponent -- cheap, and safe because whatever calls this is expected to
	// destroy this actor once baking finishes either way, so leaving HeroMeshComponent's geometry
	// empty in the meantime is harmless.
	FBuildingBakeExtractedData ExtractBakeData() const;

	// Phase 2/3 (safe on ANY thread, including a background task -- touches no UObjects at all): the
	// actual mesh-merge work (FDynamicMeshEditor::AppendMesh per bucket instance plus the hero mesh,
	// MaterialID remapping via HeroSlotToBakedSlot) that used to be inline in BakeToStaticMesh.
	static FMeshDescription BuildBakedMeshDescription(const FBuildingBakeExtractedData& Data);

	// Phase 3/3 (game thread only): builds and saves a new UStaticMesh asset from an already-merged
	// MeshDescription (see BuildBakedMeshDescription) and the same BakedMaterials list that produced
	// it (must be index-aligned with MeshDescription's polygon groups -- i.e. exactly what
	// ExtractBakeData/BuildBakedMeshDescription produced together for the same cell). Returns nullptr
	// (nothing saved) if MeshDescription has no triangles.
	static UStaticMesh* FinalizeBakedAsset(FMeshDescription&& MeshDescription, const TArray<TObjectPtr<UMaterialInterface>>& BakedMaterials, const FString& PackagePath);

	// Spawns a plain AStaticMeshActor at identity transform referencing BakedMesh (copying over
	// PoolActor's RuntimeGrid and label), then destroys PoolActor and nulls the caller's pointer to
	// it -- the actor-swap half of BakeAndReplace, factored out so the pipelined path in
	// GenerateBuildingsFromOsmFileChunked can call it once a background merge (see
	// BuildBakedMeshDescription) has finished and been finalized (FinalizeBakedAsset), without going
	// through BakeAndReplace's own (synchronous) bake step again. Same "this is a safe, genuine
	// permanent replacement, not the save-then-unload pattern" reasoning as BakeAndReplace's own
	// comment. No-op (returns nullptr) if BakedMesh is null.
	static AStaticMeshActor* ReplaceWithBakedAsset(ABuildingInstancePoolActor*& PoolActor, UStaticMesh* BakedMesh);

	// Bakes PoolActor (see BakeToStaticMesh) and, on success, spawns a plain AStaticMeshActor at
	// identity transform referencing the result (copying over PoolActor's RuntimeGrid), then
	// destroys PoolActor and nulls the caller's pointer to it. This is a genuine, permanent content
	// replacement -- PoolActor's saved package (if any) is meant to stop existing, superseded by the
	// new actor and baked asset -- NOT the "save now, unload, expect it back later" pattern
	// FBuildingActorPersistence's header comment warns is unsafe for a World-Partition-saved actor;
	// destroying an actor nobody needs to reload is an ordinary, safe editor operation. Returns the
	// new actor, or nullptr (PoolActor left untouched, not destroyed) if PoolActor had no geometry to
	// bake.
	static AStaticMeshActor* BakeAndReplace(ABuildingInstancePoolActor*& PoolActor, const FString& PackagePath);

	// Editor-only. A reasonable default BakeToStaticMesh PackagePath:
	// /Game/GeneratedBuildings/<LevelName>/SM_<SanitizedActorLabel>. Shared by the Tools-menu bake
	// action and the generation-time bBakeToStaticMeshPerCell option so both use the same
	// convention; callers are free to pass a different path to BakeToStaticMesh directly instead.
	FString MakeDefaultBakedAssetPath() const;

	// Records this cell's resolved volumes/config, so it can later be regenerated on demand (see
	// RegenerateFromSource) to apply a per-building customization. Set once, right after this pool is
	// spawned and populated, by whichever generation entry point produced it -- see
	// UBuildingGenerationLibrary::GenerateBuildingsFromOsmFileChunked. Left empty (regeneration is a
	// no-op) for pools that get baked to a static mesh per cell -- once flattened there is nothing left
	// to regenerate.
	UPROPERTY()
	TArray<FGrammarBuildingVolume> SourceVolumes;

	UPROPERTY()
	FBuildingGrammarConfig SourceConfig;

	// Per-building overrides, keyed by FGrammarBuildingVolume::SourceName. Only entries with an actual
	// override need to be present; a building with no entry here regenerates using its own VolumeTags/
	// SourceConfig unmodified.
	UPROPERTY()
	TMap<FString, FBuildingCustomizationOverride> BuildingOverrides;

	// Sets (or clears, if Override is default-constructed) this building's override entry. Does not
	// itself trigger regeneration -- callers that want the change to become visible must call
	// RegenerateFromSource() afterward (kept separate so a UI can batch several SetBuildingOverride
	// calls before paying for one regeneration).
	void SetBuildingOverride(const FString& SourceName, const FBuildingCustomizationOverride& Override);

	// Rebuilds this pool's entire cell from SourceVolumes/SourceConfig, applying each
	// building's BuildingOverrides entry (if any): TagOverrides are merged onto that building's own
	// VolumeTags (override wins on conflict), and a non-empty ForcedStyleName swaps in a SourceConfig
	// copy whose Styles array contains only the matching-named FFacadeStyleConfig -- both feed
	// FBuildingGrammarEngine::GenerateBuildingSpec's existing (Tags, Config) inputs unchanged, so
	// style/tag selection logic itself needs no special-casing here. Destroys every existing HISM
	// bucket and the hero mesh component first (this cell's entire generated geometry is rebuilt from
	// scratch, not incrementally patched) then regenerates every SourceVolumes entry exactly as the
	// original generation loop did. No-op if SourceVolumes is empty (nothing to regenerate from, e.g.
	// a baked-then-replaced cell, or an actor from before this feature existed).
	void RegenerateFromSource();

private:
	static FString MakeBucketKey(const FString& Role, const FString& VariantKey);

	UPROPERTY()
	TMap<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Buckets;

	UPROPERTY()
	TMap<FString, FBuildingMaterialResolveKey> BucketMaterialKeys;

	UPROPERTY()
	TObjectPtr<UDynamicMeshComponent> HeroMeshComponent;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> HeroMaterials;

	UPROPERTY()
	TArray<FBuildingMaterialResolveKey> HeroMaterialKeys;
};
