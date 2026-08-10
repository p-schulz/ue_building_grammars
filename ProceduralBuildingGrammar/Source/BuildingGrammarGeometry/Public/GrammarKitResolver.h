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
// ResolveMaterial caches one UMaterialInstanceDynamic per distinct (Role, MaterialName) pair
// (first Color requested for that pair wins if the same pair is later requested with a different
// color) -- this deliberately matches blender_adapter.py's own behavior, where materials are
// shared globally by name and only "upgraded" in place for texture changes, not recolored per
// caller. Each MID is a child of GetOrCreateRoleMaterial(Role) (GrammarKitAssetBuilder.h) -- a
// persistent, artist-editable UMaterialInstanceConstant per Role -- rather than of the raw master
// material, and only ever overrides BaseColor: Roughness/Metallic and anything else (textures,
// custom parameters) come from whatever that role instance is set to, so hand-tuning it in the
// Material Instance Editor actually sticks across regeneration instead of being clobbered by a
// runtime override.
class BUILDINGGRAMMARGEOMETRY_API FGrammarKitResolver
{
public:
	static UStaticMesh* ResolveKitMesh(const FString& Role, const FString& VariantKey);
	static UMaterialInterface* ResolveMaterial(const FString& Role, const FString& MaterialName, const FLinearColor& Color);
};
