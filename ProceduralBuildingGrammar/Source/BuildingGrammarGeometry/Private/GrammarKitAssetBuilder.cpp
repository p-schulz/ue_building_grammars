#include "GrammarKitAssetBuilder.h"
#include "Misc/PackageName.h"

namespace
{
	// "window_frame" -> "WindowFrame". Keeps role-derived asset names Content-Browser-friendly
	// regardless of how a Role string happens to be spelled (snake_case throughout the grammar
	// engine today, but this doesn't assume that).
	FString SanitizeRoleForAssetName(const FString& Role)
	{
		FString Result;
		bool bCapitalizeNext = true;
		for (const TCHAR Ch : Role)
		{
			if (FChar::IsAlnum(Ch))
			{
				Result.AppendChar(bCapitalizeNext ? FChar::ToUpper(Ch) : Ch);
				bCapitalizeNext = false;
			}
			else
			{
				bCapitalizeNext = true;
			}
		}
		return Result.IsEmpty() ? TEXT("Default") : Result;
	}
}

FString FGrammarKitAssetBuilder::GetRoleMaterialPath(const FString& Role)
{
	return FString::Printf(TEXT("/ProceduralBuildingGrammar/Kits/Materials/MI_%s"), *SanitizeRoleForAssetName(Role));
}

#if WITH_EDITOR

#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "MaterialEditingLibrary.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "GrammarMaterialProperties.h"

namespace
{
	// 8 corners of a unit box centered at local origin (spans -0.5..0.5 on each local axis),
	// matching FGrammarOrientedBox's corner layout (bottom 0-3, top 4-7) for consistency with the
	// rest of the port. Must be centered, not bottom-anchored: MakeBoxPlacement
	// (BuildingGrammarCore's GrammarPlacementHelpers.h) sets each instance's Location to the
	// box's world-space *center* and Scale to (Width, Depth, Height), which only reconstructs the
	// intended box if the source mesh's own local origin is that box's center.
	const FVector3f GUnitBoxCorners[8] = {
		FVector3f(-0.5f, -0.5f, -0.5f), FVector3f(0.5f, -0.5f, -0.5f), FVector3f(0.5f, 0.5f, -0.5f), FVector3f(-0.5f, 0.5f, -0.5f),
		FVector3f(-0.5f, -0.5f, 0.5f), FVector3f(0.5f, -0.5f, 0.5f), FVector3f(0.5f, 0.5f, 0.5f), FVector3f(-0.5f, 0.5f, 0.5f)
	};
	// Index order per face -- see the long history note below before touching this again.
	//
	// History: the box originally shipped with this exact table and looked inside-out (reported as
	// "flipped faces/normals" on windows/roof details). A hand-computed fix reversed every face's
	// cyclic order on the theory that cross(V[1]-V[0], V[2]-V[0]) should point outward for a correct
	// face, verified (twice, independently) as pure vector arithmetic. That fix was CONFIRMED WRONG
	// by direct empirical test: opening the baked SM_GrammarUnitBox in the Static Mesh Editor with
	// "Show Normals" enabled showed the box was still inside-out (background grid visible through
	// the front faces -- the outward side was being backface-culled, meaning the winding, not just
	// the stored normal, was still wrong). Since a quad has exactly two possible windings and the
	// "corrected" one was empirically wrong, this reverts to the ORIGINAL table below -- i.e. the
	// "outward = cross(V1-V0,V2-V0)" assumption behind the reversal was itself backward for however
	// this project's build pipeline actually determines front-facing (not simple hand-derivable
	// vector math; do not re-derive this by hand again without an empirical Show Normals check to
	// back it up). If this table is ever touched again, verify with Show Normals before trusting any
	// hand math -- see this same reasoning error's writeup in the conversation history if available.
	const int32 GUnitBoxFaces[6][4] = {
		{ 0, 1, 2, 3 }, { 4, 7, 6, 5 }, { 0, 4, 5, 1 }, { 1, 5, 6, 2 }, { 2, 6, 7, 3 }, { 3, 7, 4, 0 }
	};

	UPackage* CreateAssetPackage(const TCHAR* PackagePath)
	{
		UPackage* Package = CreatePackage(PackagePath);
		Package->FullyLoad();
		return Package;
	}

	template <typename TAssetType>
	bool SaveAssetPackage(UPackage* Package, TAssetType* Asset, const TCHAR* PackagePath)
	{
		Asset->PostEditChange();
		FAssetRegistryModule::AssetCreated(Asset);
		Package->MarkPackageDirty();

		const FString FileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
	}

	// Every kit part is rendered through a UHierarchicalInstancedStaticMeshComponent (see
	// ABuildingInstancePoolActor::AddInstance) on a Nanite-enabled mesh (GetOrCreateUnitBoxMesh),
	// so this material needs both usage flags set before it's ever assigned to one of those
	// components -- otherwise the engine sets them reactively on first use (see the MapCheck
	// "was missing the usage flag" warning), which both forces an unplanned mid-generation shader/
	// RTPSO recompile and leaves the change unsaved on disk, so it repeats every editor session
	// until the asset happens to get re-saved some other way. Returns true if either flag was
	// actually newly set (i.e. the caller needs to save Material), false if both were already set.
	bool EnsureMaterialUsageFlags(UMaterial* Material)
	{
		const bool bHadInstancedStaticMeshes = Material->bUsedWithInstancedStaticMeshes != 0;
		const bool bHadNanite = Material->bUsedWithNanite != 0;
		Material->SetMaterialUsage(MATUSAGE_InstancedStaticMeshes);
		Material->SetMaterialUsage(MATUSAGE_Nanite);
		return !bHadInstancedStaticMeshes || !bHadNanite;
	}
}

const TCHAR* FGrammarKitAssetBuilder::GetUnitBoxMeshPath()
{
	return TEXT("/ProceduralBuildingGrammar/Kits/SM_GrammarUnitBox");
}

const TCHAR* FGrammarKitAssetBuilder::GetMasterMaterialPath()
{
	return TEXT("/ProceduralBuildingGrammar/Kits/M_GrammarKit");
}

UStaticMesh* FGrammarKitAssetBuilder::GetOrCreateUnitBoxMesh()
{
	const TCHAR* PackagePath = GetUnitBoxMeshPath();
	if (UStaticMesh* Existing = LoadObject<UStaticMesh>(nullptr, PackagePath))
	{
		return Existing;
	}

	FMeshDescription MeshDescription;
	FStaticMeshAttributes Attributes(MeshDescription);
	Attributes.Register();

	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> InstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector2f> InstanceUVs = Attributes.GetVertexInstanceUVs();
	TPolygonGroupAttributesRef<FName> GroupMaterialSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();

	const FPolygonGroupID PolygonGroup = MeshDescription.CreatePolygonGroup();
	GroupMaterialSlotNames[PolygonGroup] = FName(TEXT("Kit"));

	FVertexID VertexIDs[8];
	for (int32 Index = 0; Index < 8; ++Index)
	{
		VertexIDs[Index] = MeshDescription.CreateVertex();
		VertexPositions[VertexIDs[Index]] = GUnitBoxCorners[Index];
	}

	static const FVector2f FaceUVs[4] = { FVector2f(0, 0), FVector2f(1, 0), FVector2f(1, 1), FVector2f(0, 1) };

	for (const int32(&Face)[4] : GUnitBoxFaces)
	{
		const FVector3f EdgeA = GUnitBoxCorners[Face[1]] - GUnitBoxCorners[Face[0]];
		const FVector3f EdgeB = GUnitBoxCorners[Face[2]] - GUnitBoxCorners[Face[0]];
		// GUnitBoxFaces' index order is winding for CreatePolygon/triangulation (backface culling)
		// -- confirmed correct as-is by an empirical Show Normals test (see that array's comment).
		// The stored *shading* normal is a separate, independently-consumed attribute: cross(EdgeA,
		// EdgeB) -- the same order that produces correct winding -- was confirmed WRONG for shading
		// by a World Normal buffer visualization (geometry correctly visible/not culled, but lit as
		// if facing inward). Winding and the shading-normal sign are apparently not derived from each
		// other by this project's build pipeline, so they're computed independently here: negate the
		// winding-order cross product rather than reversing GUnitBoxFaces itself, which would fix
		// shading at the cost of breaking culling again (see the array's comment for that history).
		const FVector3f Normal = -FVector3f::CrossProduct(EdgeA, EdgeB).GetSafeNormal();

		TArray<FVertexInstanceID> InstanceIDs;
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			const FVertexInstanceID InstanceID = MeshDescription.CreateVertexInstance(VertexIDs[Face[Corner]]);
			InstanceNormals[InstanceID] = Normal;
			InstanceUVs[InstanceID] = FaceUVs[Corner];
			InstanceIDs.Add(InstanceID);
		}
		MeshDescription.CreatePolygon(PolygonGroup, InstanceIDs);
	}

	UPackage* Package = CreateAssetPackage(PackagePath);
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, FName(TEXT("SM_GrammarUnitBox")), RF_Public | RF_Standalone);
	StaticMesh->GetStaticMaterials().Add(FStaticMaterial());
	StaticMesh->GetNaniteSettings().bEnabled = true;

	FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();
	SourceModel.BuildSettings.bRecomputeNormals = false;
	SourceModel.BuildSettings.bRecomputeTangents = true;
	SourceModel.BuildSettings.bGenerateLightmapUVs = true;

	TArray<const FMeshDescription*> MeshDescriptionPtrs;
	MeshDescriptionPtrs.Add(&MeshDescription);
	StaticMesh->BuildFromMeshDescriptions(MeshDescriptionPtrs);

	SaveAssetPackage(Package, StaticMesh, PackagePath);
	return StaticMesh;
}

UMaterial* FGrammarKitAssetBuilder::GetOrCreateMasterMaterial()
{
	const TCHAR* PackagePath = GetMasterMaterialPath();
	if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, PackagePath))
	{
		// Self-healing for an asset baked before these flags were set proactively (see
		// EnsureMaterialUsageFlags's comment) -- re-saves once so this doesn't re-trigger every
		// session.
		if (EnsureMaterialUsageFlags(Existing))
		{
			Existing->PreEditChange(nullptr);
			UMaterialEditingLibrary::RecompileMaterial(Existing);
			SaveAssetPackage(Existing->GetOutermost(), Existing, PackagePath);
		}
		return Existing;
	}

	UPackage* Package = CreateAssetPackage(PackagePath);
	UMaterial* Material = NewObject<UMaterial>(Package, FName(TEXT("M_GrammarKit")), RF_Public | RF_Standalone);

	UMaterialExpressionVectorParameter* BaseColorExpr = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass(), -400, 0));
	BaseColorExpr->ParameterName = TEXT("BaseColor");
	BaseColorExpr->DefaultValue = FLinearColor::White;

	UMaterialExpressionScalarParameter* RoughnessExpr = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), -400, 150));
	RoughnessExpr->ParameterName = TEXT("Roughness");
	RoughnessExpr->DefaultValue = 0.58f;

	UMaterialExpressionScalarParameter* MetallicExpr = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), -400, 250));
	MetallicExpr->ParameterName = TEXT("Metallic");
	MetallicExpr->DefaultValue = 0.0f;

	UMaterialEditingLibrary::ConnectMaterialProperty(BaseColorExpr, TEXT(""), EMaterialProperty::MP_BaseColor);
	UMaterialEditingLibrary::ConnectMaterialProperty(RoughnessExpr, TEXT(""), EMaterialProperty::MP_Roughness);
	UMaterialEditingLibrary::ConnectMaterialProperty(MetallicExpr, TEXT(""), EMaterialProperty::MP_Metallic);

	EnsureMaterialUsageFlags(Material);

	Material->PreEditChange(nullptr);
	UMaterialEditingLibrary::RecompileMaterial(Material);

	SaveAssetPackage(Package, Material, PackagePath);
	return Material;
}

UMaterialInstanceConstant* FGrammarKitAssetBuilder::GetOrCreateRoleMaterial(const FString& Role)
{
	const FString PackagePathStr = GetRoleMaterialPath(Role);
	const TCHAR* PackagePath = *PackagePathStr;
	if (UMaterialInstanceConstant* Existing = LoadObject<UMaterialInstanceConstant>(nullptr, PackagePath))
	{
		return Existing;
	}

	UMaterial* MasterMaterial = GetOrCreateMasterMaterial();
	if (!MasterMaterial)
	{
		return nullptr;
	}

	UPackage* Package = CreateAssetPackage(PackagePath);
	UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(Package, FName(*FPackageName::GetShortName(PackagePath)), RF_Public | RF_Standalone);
	Instance->SetParentEditorOnly(MasterMaterial);
	// Seeded once as a starting point only -- see this function's header comment. Whatever value
	// this instance ends up holding (left as-is or hand-tuned later) is what generated buildings
	// actually render with, since FGrammarKitResolver's runtime per-building instances only ever
	// override BaseColor.
	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Roughness")), FGrammarMaterialProperties::RoughnessForMaterialName(Role));
	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Metallic")), FGrammarMaterialProperties::MetallicForMaterialName(Role));

	SaveAssetPackage(Package, Instance, PackagePath);
	return Instance;
}

#else // !WITH_EDITOR

UStaticMesh* FGrammarKitAssetBuilder::GetOrCreateUnitBoxMesh() { return nullptr; }
UMaterial* FGrammarKitAssetBuilder::GetOrCreateMasterMaterial() { return nullptr; }
UMaterialInstanceConstant* FGrammarKitAssetBuilder::GetOrCreateRoleMaterial(const FString&) { return nullptr; }
const TCHAR* FGrammarKitAssetBuilder::GetUnitBoxMeshPath() { return TEXT("/ProceduralBuildingGrammar/Kits/SM_GrammarUnitBox"); }
const TCHAR* FGrammarKitAssetBuilder::GetMasterMaterialPath() { return TEXT("/ProceduralBuildingGrammar/Kits/M_GrammarKit"); }

#endif // WITH_EDITOR
