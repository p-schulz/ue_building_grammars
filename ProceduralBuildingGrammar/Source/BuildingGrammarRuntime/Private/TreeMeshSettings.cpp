#include "TreeMeshSettings.h"
#include "Engine/StaticMesh.h"

const TArray<TSoftObjectPtr<UStaticMesh>>& UTreeMeshSettings::GetMeshesForType(EGrammarTreeType Type) const
{
	switch (Type)
	{
	case EGrammarTreeType::FruitTree:
		return FruitTreeMeshes;
	case EGrammarTreeType::Broadleaf:
		return BroadleafTreeMeshes;
	case EGrammarTreeType::Unknown:
	default:
		return UnknownTreeMeshes;
	}
}

UStaticMesh* UTreeMeshSettings::PickMeshForType(EGrammarTreeType Type, FRandomStream& Stream) const
{
	const TArray<TSoftObjectPtr<UStaticMesh>>& Candidates = GetMeshesForType(Type);
	if (Candidates.IsEmpty())
	{
		return nullptr;
	}
	const int32 Index = Stream.RandRange(0, Candidates.Num() - 1);
	return Candidates[Index].LoadSynchronous();
}

double UTreeMeshSettings::PickScale(FRandomStream& Stream) const
{
	const double Lo = FMath::Min(MinScale, MaxScale);
	const double Hi = FMath::Max(MinScale, MaxScale);
	return Stream.FRandRange(Lo, Hi);
}
