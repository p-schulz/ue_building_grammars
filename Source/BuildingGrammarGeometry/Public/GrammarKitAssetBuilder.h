#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterial;

// Editor-only: bakes the two assets FGrammarKitResolver needs, once, under
// /ProceduralBuildingGrammar/Kits/ -- a shared 1x1x1 unit-box Nanite UStaticMesh
// (SM_GrammarUnitBox) and a minimal BaseColor/Roughness/Metallic master material (M_GrammarKit).
// Both functions are idempotent: if the asset already exists on disk, they load and return it
// instead of rebuilding.
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
class FGrammarKitAssetBuilder
{
public:
	static UStaticMesh* GetOrCreateUnitBoxMesh();
	static UMaterial* GetOrCreateMasterMaterial();

	// Package paths, exposed so FGrammarKitResolver's packaged-game (non-editor) load path uses
	// the exact same location these functions bake to.
	static const TCHAR* GetUnitBoxMeshPath();
	static const TCHAR* GetMasterMaterialPath();
};
