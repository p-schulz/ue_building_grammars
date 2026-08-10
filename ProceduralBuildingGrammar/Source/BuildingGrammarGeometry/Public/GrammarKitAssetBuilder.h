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

	// One persistent UMaterialInstanceConstant per Role (the grammar engine's placement/hero-mesh
	// role vocabulary -- "facade", "roof", "window", "window_frame", "door", "antenna", ... -- see
	// FGrammarBuildingSpec's comment), parented to GetOrCreateMasterMaterial(). Idempotent like the
	// other GetOrCreate* functions here: once GetRoleMaterialPath(Role) exists on disk, this just
	// loads and returns it, so it's safe to open in the Material Instance Editor and hand-tune
	// (swap textures, adjust the grunge/leak graph, change Roughness/Metallic, add new parameters)
	// -- those edits persist across regeneration. On first creation only, Roughness/Metallic are
	// seeded from FGrammarMaterialProperties (keyed by Role, as a reasonable starting point, not a
	// perfect match for every role) as defaults; FGrammarKitResolver's runtime per-building
	// instances are children of this asset and only ever override BaseColor, so this instance's own
	// parameter values (whatever you leave them at) are what generated buildings actually render
	// with for everything else.
	static UMaterialInstanceConstant* GetOrCreateRoleMaterial(const FString& Role);

	// One persistent UMaterialInstanceConstant per distinct (Role, MaterialName, Color) combination,
	// parented to GetOrCreateRoleMaterial(Role) with BaseColor set to Color -- a *saveable* stand-in
	// for FGrammarKitResolver::ResolveMaterial's runtime UMaterialInstanceDynamic (which is
	// Outer=GetTransientPackage()/RF_Transient and does not survive being saved as part of another
	// asset/actor's package). Used by ABuildingInstancePoolActor::BakeToStaticMesh, whose baked
	// UStaticMesh's material slots must be real, persistent references like any other static mesh
	// asset. Idempotent like the other GetOrCreate* functions here: identical (Role, MaterialName,
	// Color) combinations across different cells/bakes share one asset instead of duplicating it.
	static UMaterialInstanceConstant* GetOrCreateColorVariant(const FString& Role, const FString& MaterialName, const FLinearColor& Color);

	// Package paths, exposed so FGrammarKitResolver's packaged-game (non-editor) load path uses
	// the exact same location these functions bake to.
	static const TCHAR* GetUnitBoxMeshPath();
	static const TCHAR* GetMasterMaterialPath();
	static FString GetRoleMaterialPath(const FString& Role);
	static FString GetColorVariantPath(const FString& Role, const FString& MaterialName, const FLinearColor& Color);
};
