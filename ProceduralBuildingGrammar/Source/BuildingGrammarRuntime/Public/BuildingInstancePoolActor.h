#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Osm/BuildingPartResolver.h"
#include "Config/BuildingGrammarConfig.h"
#include "Config/RoofStyleConfig.h"
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
// slot. ResolveMaterial now always returns a persistent UMaterialInstanceConstant asset (see its own
// comment), which survives an ordinary UPROPERTY reference across save/reload fine, so this re-
// resolution mechanism (see PostLoad) is technically redundant today -- kept in place anyway as a
// harmless, idempotent safety net rather than risking a subtler removal (e.g. it still helps if an
// older-format actor references an asset under a since-renamed style/role path).
USTRUCT()
struct FBuildingMaterialResolveKey
{
	GENERATED_BODY()

	// The FFacadeStyleConfig::Name this material was resolved for -- see
	// FGrammarMeshSpec::StyleName's comment. Needed here (not just on the spec structs) so PostLoad/
	// ExtractBakeData can re-resolve/re-bake the exact same per-style asset later.
	UPROPERTY()
	FString StyleName;

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

	// Structured convenience overrides below: each just writes the matching tag key EffectiveTags
	// already gets merged into before FBuildingGrammarEngine::GenerateBuildingSpec runs (see
	// RegenerateFromSource) -- FGrammarLevels::InferLevels already reads "building:levels", and
	// GrammarEngineInternal::RoofFromTags already reads "grammar:roof:type" (a plugin-specific alias
	// alongside OSM's own "roof:shape"/"roof:type") -- so no engine/generation changes are needed,
	// this just exposes a friendly typed UI control instead of requiring TagOverrides with the exact
	// tag key spelled out by hand.
	UPROPERTY(EditAnywhere, Category = "Building")
	bool bOverrideLevels = false;

	UPROPERTY(EditAnywhere, Category = "Building", meta = (EditCondition = "bOverrideLevels", ClampMin = "1"))
	int32 Levels = 4;

	UPROPERTY(EditAnywhere, Category = "Roof")
	bool bOverrideRoofType = false;

	UPROPERTY(EditAnywhere, Category = "Roof", meta = (EditCondition = "bOverrideRoofType"))
	EGrammarRoofType RoofType = EGrammarRoofType::Flat;
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
	// caller doesn't care about this bucket surviving a save+reload cycle. StyleName is recorded
	// alongside RoleTag/VariantKey/MaterialColor for the same reload-survival reason -- see
	// FBuildingMaterialResolveKey's comment; pass an empty string if it doesn't matter (e.g. Material
	// is null).
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	void AddInstance(const FString& RoleTag, const FString& VariantKey, const FTransform& Transform, UStaticMesh* Mesh, UMaterialInterface* Material = nullptr, FLinearColor MaterialColor = FLinearColor::White, FString StyleName = FString());

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
		TFunctionRef<UMaterialInterface* (const FString& StyleName, const FString& Role, const FString& MaterialName, const FLinearColor& Color)> ResolveMaterial);

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
	void AppendHeroMesh(const UE::Geometry::FDynamicMesh3& BuiltMesh, UMaterialInterface* Material, const FString& StyleName, const FString& RoleTag, const FString& MaterialName, const FLinearColor& Color);

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

	// Lightweight counterpart to BakeToStaticMesh/BakeAndReplace, analogous to FlexNetwork's "Bake
	// Network To Level": creates no new assets and does no mesh-merge/Nanite/lightmap work. Just
	// destroys this pool's derived HISM bucket and hero-mesh component data -- the per-instance
	// transform arrays and hero mesh geometry buffers that make up the bulk of what a saved level
	// actually stores for a generated cell -- while leaving SourceVolumes/SourceConfig/
	// BuildingOverrides (the small, authored data those components were built from) untouched.
	// PostRegisterAllComponents rebuilds the exact same geometry via RegenerateFromSource() the next
	// time this actor loads, the same way AFlexNetworkBakeActor::RestoreToSubsystem reconstructs the
	// road network from its own small authored snapshot. Unlike BakeToStaticMesh, this keeps full
	// per-building edit/regenerate capability -- it only shrinks what's serialized in the meantime.
	// No-op if SourceVolumes is empty (nothing to regenerate from later, e.g. an already-baked-to-
	// static-mesh cell, or a hand-placed actor with no source data).
	UFUNCTION(CallInEditor, Category = "Building Grammar", meta = (DisplayName = "Bake to Level (Lightweight)"))
	void BakeToLevelLightweight();

	// Regenerates from SourceVolumes/SourceConfig exactly once per actor/world instance if this pool
	// was lightweight-baked (see BakeToLevelLightweight) and hasn't regenerated in this world yet --
	// mirrors AFlexNetworkBakeActor::PostRegisterAllComponents/RestoreToSubsystem's own once-per-world
	// restore. A single hook (not also BeginPlay, unlike FlexNetworkBakeActor) is enough here: unlike
	// FlexNetwork's restore, RegenerateFromSource has no subsystem-readiness ordering to hedge
	// against -- it only calls into BuildingGrammarCore/BuildingGrammarGeometry, both already usable
	// as soon as this actor's own components register.
	virtual void PostRegisterAllComponents() override;

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
	// BuildBakedMeshDescription/FinalizeBakedAsset below -- used by BakeAndReplace, i.e. the
	// standalone "Save to Static Meshes" menu action. The three phases are still split out
	// separately below for any caller that wants to overlap the expensive, UObject-free merge work
	// (BuildBakedMeshDescription) with other work on a background task; nothing currently does (see
	// BakeToLevelLightweight for the generation-time per-cell memory-reduction option instead, which
	// needs no such pipelining since it does no mesh-merge work at all).
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
	// it -- the actor-swap half of BakeAndReplace, factored out so a caller that already has a
	// finished, finalized bake (see BuildBakedMeshDescription/FinalizeBakedAsset) can apply it without
	// going through BakeAndReplace's own (synchronous) bake step again. Same "this is a safe, genuine
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
	// /Game/GeneratedBuildings/<LevelName>/SM_<SanitizedActorLabel>. Used by both the "Save to Static
	// Meshes" Tools-menu action and BakeAndReplace's own callers so they share one convention;
	// callers are free to pass a different path to BakeToStaticMesh directly instead. Not used by
	// BakeToLevelLightweight, which creates no asset and therefore needs no package path.
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
	//
	// O(this pool's SourceVolumes.Num()) EVERY call, regardless of how many buildings actually
	// changed -- an editor caller that invokes this once per interactive edit (place/move/delete one
	// building at a time) pays for the whole pool's generation on every single edit, which is why a
	// pool that has accumulated many buildings can make each further edit noticeably slower than the
	// last. AppendVolume below exists specifically to avoid that cost for the common "just add one new
	// building, nothing else changed" case; this method remains the only correct option whenever an
	// EXISTING building's own footprint/tags/override changed or a building was removed, since HISM
	// buckets have no per-building partial removal/replace.
	void RegenerateFromSource();

	// Same as RegenerateFromSource(), but calls OnVolumeCompleted once after each SourceVolumes entry
	// finishes (VolumesCompleted, SourceVolumes.Num(), that volume's SourceName) -- lets a caller drive
	// a cancellable FScopedSlowTask the same way
	// UBuildingGenerationLibrary::GenerateBuildingsFromOsmFileChunked's OnCellCompleted callback does.
	// Not a UFUNCTION (TFunctionRef isn't Blueprint-compatible) -- same reasoning as that function's
	// own split into a plain Blueprint-facing overload plus this progress-reporting one. Returning
	// false from the callback stops regeneration before the next volume starts; whatever already
	// regenerated stays applied, matching that same function's "not rolled back" contract.
	void RegenerateFromSource(TFunctionRef<bool(int32 VolumesCompleted, int32 TotalVolumes, const FString& CurrentSourceName)> OnVolumeCompleted);

	// Generates and appends exactly one building -- Volume, which must already be present in
	// SourceVolumes (the caller adds it there first, e.g. via SourceVolumes.Add, so a save/reload
	// still has it for a later RegenerateFromSource) -- onto this pool's EXISTING components, without
	// touching or regenerating any other building already in the pool. O(1) relative to pool size,
	// unlike RegenerateFromSource's full teardown+rebuild-everything, so placing N new buildings one at
	// a time costs O(N) total instead of O(N^2). Only safe for a genuinely new building with no prior
	// geometry of its own to replace -- see RegenerateFromSource's own comment for why any edit to an
	// EXISTING building still needs that method instead. Returns false (nothing applied, caller should
	// remove Volume from SourceVolumes again) if generation fails, e.g. a degenerate footprint.
	bool AppendVolume(const FGrammarBuildingVolume& Volume);

private:
	// Shared by RegenerateFromSource and AppendVolume: resolves Volume's effective (Tags, Config) --
	// this building's own VolumeTags/SourceConfig with its BuildingOverrides entry (if any) applied --
	// and generates+applies its spec into this pool's existing components. Does not call
	// FlushHeroMeshUpdates() itself (callers batch that once after however many volumes they process).
	// Returns false (nothing applied) if FBuildingGrammarEngine::GenerateBuildingSpec fails.
	bool GenerateAndApplyVolume(const FGrammarBuildingVolume& Volume);

	static FString MakeBucketKey(const FString& Role, const FString& VariantKey);

	// Shared teardown used by both RegenerateFromSource (which rebuilds right after) and
	// BakeToLevelLightweight (which deliberately leaves the pool empty until the next load).
	void DestroyGeneratedComponents();

	// Deliberately not reflected (see AFlexNetworkBakeActor::bRestoredThisWorld for the same
	// pattern): a PIE duplicate must start false even if the editor instance already regenerated.
	bool bRegeneratedThisWorld = false;

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
