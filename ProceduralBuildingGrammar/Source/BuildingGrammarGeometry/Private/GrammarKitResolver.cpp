#include "GrammarKitResolver.h"
#include "GrammarKitAssetBuilder.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// Shared across every call site (editor tool, generation library, streaming subsystem) within
	// one process/PIE session. Not persisted -- rebuilt from scratch each run/session, which is
	// fine since the underlying LoadObject/GetOrCreateColorVariant lookup this caches is itself
	// idempotent; this is purely a perf optimization, not a correctness requirement (see
	// GrammarKitResolver.h's header comment).
	TMap<FString, TWeakObjectPtr<UMaterialInterface>> GMaterialInstanceCache;
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

UMaterialInterface* FGrammarKitResolver::ResolveMaterial(const FString& StyleName, const FString& Role, const FString& MaterialName, const FLinearColor& Color)
{
	// Color is deliberately not part of the cache key on its own -- it's folded into MaterialName
	// upstream instead (see BuildingGrammarEngine.cpp's WallMaterialName-style per-color-variant
	// naming), matching how GetColorVariantPath already derives its own asset name from
	// (StyleName, Role, MaterialName, Color) together. Including Color here too would just be
	// redundant, not incorrect.
	const FString CacheKey = StyleName + TEXT("|") + Role + TEXT("|") + MaterialName;
	if (const TWeakObjectPtr<UMaterialInterface>* Existing = GMaterialInstanceCache.Find(CacheKey))
	{
		if (UMaterialInterface* ExistingMaterial = Existing->Get())
		{
			return ExistingMaterial;
		}
	}

#if WITH_EDITOR
	// GetOrCreateColorVariant is the single material-resolution path now, used identically by live
	// generation (here) and by ABuildingInstancePoolActor::BakeToStaticMesh -- see this class's
	// header comment for why a persistent asset replaced the former transient
	// UMaterialInstanceDynamic.
	UMaterialInterface* Material = FGrammarKitAssetBuilder::GetOrCreateColorVariant(StyleName, Role, MaterialName, Color);
#else
	// GetOrCreateColorVariant is WITH_EDITOR-only (like every other GetOrCreate* in that class --
	// baking new assets isn't possible outside the editor), so a packaged game must load the
	// already-baked asset directly by path instead.
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *FGrammarKitAssetBuilder::GetColorVariantPath(StyleName, Role, MaterialName, Color));
#endif
	if (!Material)
	{
		UE_LOG(LogTemp, Warning, TEXT("FGrammarKitResolver: material not found for style '%s' role '%s' material '%s' -- run building generation in-editor at least once so it gets baked before packaging."), *StyleName, *Role, *MaterialName);
		return nullptr;
	}

	GMaterialInstanceCache.Add(CacheKey, Material);
	return Material;
}
