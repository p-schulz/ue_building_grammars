#include "BuildingActorPersistence.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "UObject/Package.h"

bool FBuildingActorPersistence::IsWorldPartitioned(const UWorld* World)
{
	return World && World->GetSubsystem<UWorldPartitionSubsystem>() != nullptr;
}

#if WITH_EDITOR

#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"

int32 FBuildingActorPersistence::SaveActors(const TArray<AActor*>& Actors, TArray<AActor*>& OutFailedActors)
{
	if (Actors.IsEmpty())
	{
		return 0;
	}

	TArray<UPackage*> Packages;
	Packages.Reserve(Actors.Num());
	for (AActor* Actor : Actors)
	{
		if (!Actor)
		{
			continue;
		}
		Actor->SetPackageExternal(true);
		if (UPackage* ActorPackage = Actor->GetExternalPackage())
		{
			Packages.Add(ActorPackage);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("FBuildingActorPersistence: failed to create an external package for '%s'"), *Actor->GetName());
			OutFailedActors.Add(Actor);
		}
	}

	// Same internal save machinery the interactive editor Save/close-prompt flow uses -- see this
	// class's header comment for why a hand-rolled UPackage::SavePackage call turned out not to be
	// equivalent. bCheckDirty=false forces the save regardless of dirty-flag state (these packages
	// were only just created by SetPackageExternal above, so relying on dirty tracking here isn't
	// worth the risk); bPromptToSave=false runs headless, no blocking UI.
	TArray<UPackage*> FailedPackages;
	FEditorFileUtils::PromptForCheckoutAndSave(Packages, /*bCheckDirty=*/false, /*bPromptToSave=*/false, &FailedPackages);

	int32 SavedCount = 0;
	for (AActor* Actor : Actors)
	{
		if (!Actor)
		{
			continue;
		}
		UPackage* ActorPackage = Actor->GetExternalPackage();
		if (!ActorPackage)
		{
			// Already logged and added to OutFailedActors in the loop above.
			continue;
		}
		if (FailedPackages.Contains(ActorPackage))
		{
			UE_LOG(LogTemp, Warning, TEXT("FBuildingActorPersistence: failed to save external package for '%s'"), *Actor->GetName());
			OutFailedActors.Add(Actor);
			continue;
		}
		++SavedCount;
	}

	return SavedCount;
}

bool FBuildingActorPersistence::SaveLevel()
{
	return FEditorFileUtils::SaveCurrentLevel();
}

bool FBuildingActorPersistence::SaveAndReloadLevel(UWorld*& World)
{
	if (!World)
	{
		return false;
	}

	const FString PackageName = World->GetOutermost()->GetName();
	FString LevelFilename;
	if (!FPackageName::DoesPackageExist(PackageName, &LevelFilename))
	{
		UE_LOG(LogTemp, Warning, TEXT("FBuildingActorPersistence: level '%s' has never been saved to disk; cannot reload it"), *PackageName);
		return false;
	}

	if (!SaveLevel())
	{
		UE_LOG(LogTemp, Warning, TEXT("FBuildingActorPersistence: failed to save the current level before reload"));
		return false;
	}

	// Synchronous -- fully tears down the old world (destroying and GC'ing every actor currently
	// resident) and loads the new one before returning; see this class's header comment. bShowProgress
	// is false since callers drive their own FScopedSlowTask around the whole batch loop.
	if (!FEditorFileUtils::LoadMap(LevelFilename, /*LoadAsTemplate=*/false, /*bShowProgress=*/false))
	{
		UE_LOG(LogTemp, Warning, TEXT("FBuildingActorPersistence: failed to reload '%s'"), *LevelFilename);
		World = nullptr;
		return false;
	}

	World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	return World != nullptr;
}

#else // !WITH_EDITOR

int32 FBuildingActorPersistence::SaveActors(const TArray<AActor*>&, TArray<AActor*>&)
{
	return 0;
}

bool FBuildingActorPersistence::SaveLevel()
{
	return false;
}

bool FBuildingActorPersistence::SaveAndReloadLevel(UWorld*&)
{
	return false;
}

#endif // WITH_EDITOR
