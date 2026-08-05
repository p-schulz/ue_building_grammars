#include "GrammarKitAssetBuilder.h"

#if WITH_EDITOR

#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "MaterialEditingLibrary.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

namespace
{
	// 8 corners of a unit box centered at local origin (spans -0.5..0.5 on each local axis),
	// matching FGrammarOrientedBox's corner order/winding (bottom 0-3, top 4-7) for consistency
	// with the rest of the port. Must be centered, not bottom-anchored: MakeBoxPlacement
	// (BuildingGrammarCore's GrammarPlacementHelpers.h) sets each instance's Location to the
	// box's world-space *center* and Scale to (Width, Depth, Height), which only reconstructs the
	// intended box if the source mesh's own local origin is that box's center.
	const FVector3f GUnitBoxCorners[8] = {
		FVector3f(-0.5f, -0.5f, -0.5f), FVector3f(0.5f, -0.5f, -0.5f), FVector3f(0.5f, 0.5f, -0.5f), FVector3f(-0.5f, 0.5f, -0.5f),
		FVector3f(-0.5f, -0.5f, 0.5f), FVector3f(0.5f, -0.5f, 0.5f), FVector3f(0.5f, 0.5f, 0.5f), FVector3f(-0.5f, 0.5f, 0.5f)
	};
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
		const FVector3f Normal = FVector3f::CrossProduct(EdgeA, EdgeB).GetSafeNormal();

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
	StaticMesh->NaniteSettings.bEnabled = true;

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
		return Existing;
	}

	UPackage* Package = CreateAssetPackage(PackagePath);
	UMaterial* Material = NewObject<UMaterial>(Package, FName(TEXT("M_GrammarKit")), RF_Public | RF_Standalone);

	UMaterialExpressionVectorParameter* BaseColorExpr = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::NewMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass(), -400, 0));
	BaseColorExpr->ParameterName = TEXT("BaseColor");
	BaseColorExpr->DefaultValue = FLinearColor::White;

	UMaterialExpressionScalarParameter* RoughnessExpr = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::NewMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), -400, 150));
	RoughnessExpr->ParameterName = TEXT("Roughness");
	RoughnessExpr->DefaultValue = 0.58f;

	UMaterialExpressionScalarParameter* MetallicExpr = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::NewMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), -400, 250));
	MetallicExpr->ParameterName = TEXT("Metallic");
	MetallicExpr->DefaultValue = 0.0f;

	UMaterialEditingLibrary::ConnectMaterialProperty(BaseColorExpr, TEXT(""), EMaterialProperty::MP_BaseColor);
	UMaterialEditingLibrary::ConnectMaterialProperty(RoughnessExpr, TEXT(""), EMaterialProperty::MP_Roughness);
	UMaterialEditingLibrary::ConnectMaterialProperty(MetallicExpr, TEXT(""), EMaterialProperty::MP_Metallic);

	Material->PreEditChange(nullptr);
	UMaterialEditingLibrary::RecompileMaterial(Material);

	SaveAssetPackage(Package, Material, PackagePath);
	return Material;
}

#else // !WITH_EDITOR

UStaticMesh* FGrammarKitAssetBuilder::GetOrCreateUnitBoxMesh() { return nullptr; }
UMaterial* FGrammarKitAssetBuilder::GetOrCreateMasterMaterial() { return nullptr; }
const TCHAR* FGrammarKitAssetBuilder::GetUnitBoxMeshPath() { return TEXT("/ProceduralBuildingGrammar/Kits/SM_GrammarUnitBox"); }
const TCHAR* FGrammarKitAssetBuilder::GetMasterMaterialPath() { return TEXT("/ProceduralBuildingGrammar/Kits/M_GrammarKit"); }

#endif // WITH_EDITOR
