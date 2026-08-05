#include "GrammarKitResolver.h"
#include "GrammarKitAssetBuilder.h"
#include "GrammarMaterialProperties.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// Shared across every call site (editor tool, generation library, streaming subsystem) within
	// one process/PIE session. Not persisted -- a packaged game rebuilds this cache from scratch
	// each run, which is fine since MIDs are cheap and inherently runtime/session-only.
	TMap<FString, TWeakObjectPtr<UMaterialInstanceDynamic>> GMaterialInstanceCache;
}

UStaticMesh* FGrammarKitResolver::ResolveKitMesh(const FString& Role, const FString& VariantKey)
{
	// Every role shares the same unit box today -- see this file's header comment. Parameters
	// kept for a future per-role/per-dimension kit without changing every call site's signature.
	(void)Role;
	(void)VariantKey;

#if WITH_EDITOR
	return FGrammarKitAssetBuilder::GetOrCreateUnitBoxMesh();
#else
	static TWeakObjectPtr<UStaticMesh> CachedMesh;
	if (!CachedMesh.IsValid())
	{
		CachedMesh = LoadObject<UStaticMesh>(nullptr, FGrammarKitAssetBuilder::GetUnitBoxMeshPath());
		if (!CachedMesh.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("FGrammarKitResolver: kit mesh not found -- run building generation in-editor at least once so it gets baked before packaging."));
		}
	}
	return CachedMesh.Get();
#endif
}

UMaterialInterface* FGrammarKitResolver::ResolveMaterial(const FString& MaterialName, const FLinearColor& Color)
{
	if (const TWeakObjectPtr<UMaterialInstanceDynamic>* Existing = GMaterialInstanceCache.Find(MaterialName))
	{
		if (UMaterialInstanceDynamic* ExistingMID = Existing->Get())
		{
			return ExistingMID;
		}
	}

#if WITH_EDITOR
	UMaterialInterface* MasterMaterial = FGrammarKitAssetBuilder::GetOrCreateMasterMaterial();
#else
	UMaterialInterface* MasterMaterial = LoadObject<UMaterialInterface>(nullptr, FGrammarKitAssetBuilder::GetMasterMaterialPath());
#endif
	if (!MasterMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("FGrammarKitResolver: master material not found for '%s' -- run building generation in-editor at least once so it gets baked before packaging."), *MaterialName);
		return nullptr;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MasterMaterial, GetTransientPackage());
	if (!MID)
	{
		return nullptr;
	}
	MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
	MID->SetScalarParameterValue(TEXT("Roughness"), FGrammarMaterialProperties::RoughnessForMaterialName(MaterialName));
	MID->SetScalarParameterValue(TEXT("Metallic"), FGrammarMaterialProperties::MetallicForMaterialName(MaterialName));

	GMaterialInstanceCache.Add(MaterialName, MID);
	return MID;
}
