#include "GrammarKitResolver.h"
#include "GrammarKitAssetBuilder.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
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

UMaterialInterface* FGrammarKitResolver::ResolveMaterial(const FString& Role, const FString& MaterialName, const FLinearColor& Color)
{
	const FString CacheKey = Role + TEXT("|") + MaterialName;
	if (const TWeakObjectPtr<UMaterialInstanceDynamic>* Existing = GMaterialInstanceCache.Find(CacheKey))
	{
		if (UMaterialInstanceDynamic* ExistingMID = Existing->Get())
		{
			return ExistingMID;
		}
	}

#if WITH_EDITOR
	UMaterialInterface* RoleMaterial = FGrammarKitAssetBuilder::GetOrCreateRoleMaterial(Role);
#else
	UMaterialInterface* RoleMaterial = LoadObject<UMaterialInterface>(nullptr, *FGrammarKitAssetBuilder::GetRoleMaterialPath(Role));
#endif
	if (!RoleMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("FGrammarKitResolver: role material not found for role '%s' -- run building generation in-editor at least once so it gets baked before packaging."), *Role);
		return nullptr;
	}

	// Only BaseColor is overridden here -- Roughness/Metallic and anything else (textures, custom
	// parameters) come from RoleMaterial itself, which is the persistent, artist-editable instance
	// (see GetOrCreateRoleMaterial's comment). Setting them here too would silently clobber any
	// hand-tuning done on that asset every time generation runs.
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(RoleMaterial, GetTransientPackage());
	if (!MID)
	{
		return nullptr;
	}
	MID->SetVectorParameterValue(TEXT("BaseColor"), Color);

	GMaterialInstanceCache.Add(CacheKey, MID);
	return MID;
}
