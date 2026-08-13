#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;

// Turns an FGrammarPlacementRecord's (Role, VariantKey, Color) into an actual mesh + material to
// instance -- the piece that makes every non-hero element (windows, doors, roof tiles, balconies,
// antennas, ...) actually render, not just compute a correct placement transform. Safe to call
// from both editor and runtime code:
//  - In the editor, both functions bake the shared kit assets on first use (see
//    GrammarKitAssetBuilder.h, WITH_EDITOR-only) under /ProceduralBuildingGrammar/Kits/, then just
//    load-and-return them on every subsequent call.
//  - In a packaged (non-editor) game, they only *load* those same assets by path -- baking new
//    UStaticMesh/UMaterial assets isn't possible outside the editor. This means a level shipping
//    in a packaged build needs generation to have been run at least once in the editor (which
//    bakes and saves the kit assets, and -- as a normal referenced asset once used in a level --
//    they'll cook and ship with the game from then on) before packaging; ResolveKitMesh/
//    ResolveMaterial return null and log a warning if the assets were never baked.
//
// Every role currently resolves to the exact same shared unit-box mesh (only the material
// differs) -- see BuildingGrammarCore's GrammarPlacementHelpers.h for why a single 1x1x1 box,
// non-uniformly scaled per instance via each placement's FTransform, covers every role the
// grammar engine emits a placement for today.
//
// ResolveMaterial resolves to a persistent UMaterialInstanceConstant
// (GrammarKitAssetBuilder.h's GetOrCreateColorVariant), one per distinct (StyleName, Role,
// MaterialName, Color) combination -- StyleName (FGrammarMeshSpec::StyleName/
// FGrammarPlacementRecord::StyleName, i.e. the FFacadeStyleConfig::Name that produced this mesh/
// placement) keeps different building styles' materials as separate, independently art-directable
// assets under their own Content Browser sub-directory rather than one instance shared across every
// style that happens to reuse the same Role/MaterialName. This is a real, saveable asset -- not a
// transient UMaterialInstanceDynamic -- so it survives save/reload as a normal asset reference and
// is exactly what's baked into a static mesh's material slot when a cell is baked
// (ABuildingInstancePoolActor::BakeToStaticMesh calls GetOrCreateColorVariant directly for the same
// reason). Each instance is a child of GetOrCreateRoleMaterial(StyleName, Role)
// (GrammarKitAssetBuilder.h) -- a persistent, artist-editable UMaterialInstanceConstant per
// (StyleName, Role) -- rather than of the raw master material, and only ever overrides BaseColor:
// Roughness/Metallic and anything else (textures, custom parameters) come from whatever that
// (StyleName, Role) instance is set to, so hand-tuning it in the Material Instance Editor actually
// sticks across regeneration instead of being clobbered by a runtime override. Results are cached
// in-memory by the full (StyleName, Role, MaterialName) key purely to avoid repeated LoadObject/
// path-string work within one session -- the underlying asset lookup is itself idempotent, so this
// cache is a performance optimization, not a correctness requirement.
class BUILDINGGRAMMARGEOMETRY_API FGrammarKitResolver
{
public:
	static UStaticMesh* ResolveKitMesh(const FString& Role, const FString& VariantKey);
	static UMaterialInterface* ResolveMaterial(const FString& StyleName, const FString& Role, const FString& MaterialName, const FLinearColor& Color);
};
