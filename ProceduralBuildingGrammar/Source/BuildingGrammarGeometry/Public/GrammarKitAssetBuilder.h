#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterial;
class UMaterialInstanceConstant;

// Editor-only: bakes the assets FGrammarKitResolver needs, once, under
// /ProceduralBuildingGrammar/Kits/ -- a shared 1x1x1 unit-box Nanite UStaticMesh
// (SM_GrammarUnitBox), a master material (M_GrammarKit), and one persistent, artist-editable
// UMaterialInstanceConstant per Role (see GetOrCreateRoleMaterial). All are idempotent: if the
// asset already exists on disk, they load and return it instead of rebuilding, so hand-edits (e.g.
// extending M_GrammarKit's node graph, or tuning a role instance's textures/parameters in the
// Material Instance Editor) are never overwritten by generation.
//
// THIS IS THE HIGHEST-API-RISK FILE IN THE WHOLE PROJECT. Constructing a UMaterial's node graph
// programmatically (UMaterialEditingLibrary::NewMaterialExpression/ConnectMaterialProperty) and
// building a UStaticMesh from an FMeshDescription with Nanite enabled are both written from
// recollection of the general shape of these APIs, not verified against engine headers or a
// compiler -- more so even than GrammarDynamicMeshBuilder.cpp's attribute-overlay calls, which at
// least operate on a single well-known type (FDynamicMesh3). Expect exact method/field names here
// (NaniteSettings.bEnabled, BuildFromMeshDescriptions' parameter list, FSavePackageArgs, the
// UMaterialEditingLibrary call shapes) to need correction against your UE5.6 headers. If this
// module fails to compile, start here.
//
// Was never marked for DLL export until ABuildingInstancePoolActor::BakeToStaticMesh
// (BuildingGrammarRuntime) became this class's first cross-module caller -- everything before that
// only ever called it from within this same module (e.g. GrammarKitResolver.cpp), so the missing
// BUILDINGGRAMMARGEOMETRY_API silently never mattered until then (link error, not a compile error).
class BUILDINGGRAMMARGEOMETRY_API FGrammarKitAssetBuilder
{
public:
	static UStaticMesh* GetOrCreateUnitBoxMesh();
	static UMaterial* GetOrCreateMasterMaterial();

	// One persistent UMaterialInstanceConstant per (StyleName, Role) pair -- StyleName is the
	// FFacadeStyleConfig::Name the mesh/placement this material is being resolved for was actually
	// generated from (FGrammarMeshSpec::StyleName/FGrammarPlacementRecord::StyleName), Role is the
	// grammar engine's placement/hero-mesh role vocabulary ("facade", "roof", "window",
	// "window_frame", "door", "antenna", ... -- see FGrammarBuildingSpec's comment). Stored under a
	// sub-directory per style (GetRoleMaterialPath) so e.g. an "apartments" style's window material
	// and an "office" style's window material are separate, independently art-directable assets
	// instead of one shared-by-role instance. Parented, on first creation, to
	// GetOrCreateMaterialFamily(ClassifyMaterialFamily(MaterialName)) (GrammarKitAssetBuilder.cpp) --
	// NOT the raw master material directly -- so e.g. every style/role whose MaterialName reads as
	// "stone"-like shares one hand-editable "Stone" family asset's textures/parameters as a starting
	// point, while still getting its own independently art-directable (StyleName, Role) instance on
	// top. MaterialName is consulted ONLY to pick this parent on first creation -- it is NOT part of
	// this asset's identity/path (GetRoleMaterialPath takes no MaterialName), so calling this again
	// later with a different MaterialName for the same (StyleName, Role) has no effect once the asset
	// already exists (matches every other GetOrCreate* here: idempotent, existing assets untouched).
	// Idempotent like the other GetOrCreate* functions here: once GetRoleMaterialPath(StyleName,
	// Role) exists on disk, this just loads and returns it, so it's safe to open in the Material
	// Instance Editor and hand-tune (swap textures, adjust the grunge/leak graph, change Roughness/
	// Metallic, add new parameters) -- those edits persist across regeneration. On first creation
	// only, Roughness/Metallic are seeded from FGrammarMaterialProperties (keyed by Role only, not by
	// style, as a reasonable starting point, not a perfect match for every role) as defaults;
	// FGrammarKitResolver's per-building instances (GetOrCreateColorVariant) are children of this
	// asset and only ever override BaseColor, so this instance's own parameter values (whatever you
	// leave them at) are what generated buildings actually render with for everything else.
	static UMaterialInstanceConstant* GetOrCreateRoleMaterial(const FString& StyleName, const FString& Role, const FString& MaterialName);

	// One persistent UMaterialInstanceConstant per distinct (StyleName, Role, MaterialName, Color)
	// combination, parented to GetOrCreateRoleMaterial(StyleName, Role) with BaseColor set to Color.
	// This is the actual per-building/per-placement material FGrammarKitResolver::ResolveMaterial
	// resolves to (both for live generation and for ABuildingInstancePoolActor::BakeToStaticMesh,
	// whose baked UStaticMesh's material slots must be real, persistent references like any other
	// static mesh asset) -- a genuine content asset, not a transient UMaterialInstanceDynamic, so it
	// shows up in the Content Browser under its style's sub-directory and survives save/reload like
	// any other asset reference. Idempotent like the other GetOrCreate* functions here: identical
	// (StyleName, Role, MaterialName, Color) combinations share one asset instead of duplicating it.
	static UMaterialInstanceConstant* GetOrCreateColorVariant(const FString& StyleName, const FString& Role, const FString& MaterialName, const FLinearColor& Color);

	// Package paths, exposed so FGrammarKitResolver's packaged-game (non-editor) load path uses
	// the exact same location these functions bake to. Empty StyleName falls back to "Default".
	static const TCHAR* GetUnitBoxMeshPath();
	static const TCHAR* GetMasterMaterialPath();
	static FString GetRoleMaterialPath(const FString& StyleName, const FString& Role);
	static FString GetColorVariantPath(const FString& StyleName, const FString& Role, const FString& MaterialName, const FLinearColor& Color);

private:
	// Everything below is an implementation detail of GetOrCreateRoleMaterial's parent selection
	// (see its own comment) -- not needed by any other caller in or outside this module (unlike
	// GetOrCreateColorVariant, a packaged game never loads a family material directly by path, only
	// ever a fully-resolved color variant), so kept private rather than adding more public surface.
	static UMaterialInstanceConstant* GetOrCreateMaterialFamily(const FString& FamilyName);
	static FString GetMaterialFamilyPath(const FString& FamilyName);
	static FString ClassifyMaterialFamily(const FString& MaterialName);
};
