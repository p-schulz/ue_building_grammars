#include "StreetFurnitureMeshSettings.h"
#include "Engine/StaticMesh.h"

UStaticMesh* UStreetFurnitureMeshSettings::PickMeshForCategory(const FString& Category, FRandomStream& Stream) const
{
	for (const FStreetFurnitureMeshEntry& Entry : Categories)
	{
		if (!Entry.Category.Equals(Category, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (Entry.Meshes.IsEmpty())
		{
			return nullptr;
		}
		const int32 Index = Stream.RandRange(0, Entry.Meshes.Num() - 1);
		return Entry.Meshes[Index].LoadSynchronous();
	}
	return nullptr;
}
