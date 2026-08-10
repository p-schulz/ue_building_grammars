#pragma once

#include "CoreMinimal.h"

class UWorld;

// One File Per Actor (OFPA) save helper, plus the level-reload mechanism used to actually free
// memory during a batch generation run into a World-Partition level -- lets a batch generation
// loop bound its peak memory to roughly "one batch's worth" rather than keeping every generated
// actor resident for the whole run. See
// UBuildingGenerationLibrary::GenerateBuildingsFromOsmFileChunked's bSaveAndUnloadPerCell param for
// the actual caller.
//
// History, so the next person doesn't repeat this (two dead ends before landing here):
//
// 1. The first version called AActor::SetPackageExternal(true) followed directly by
//    UPackage::SavePackage() per actor -- that call shape matches Epic's own first-time
//    actor-externalization code (WorldPartitionConvertCommandlet.cpp), and the resulting .uasset
//    files land in exactly the right __ExternalActors__/<Level>/... location, but in practice World
//    Partition never picked them up: no cell ever appeared (even across a full editor restart), and
//    the files were later silently deleted by the engine's own interactive "save on close" flow as
//    orphaned/untracked content. Fixed by switching to FEditorFileUtils::PromptForCheckoutAndSave --
//    the same internal machinery the interactive Save/close-prompt path already uses -- which
//    registers correctly (confirmed: WP minimap shows correct cell rectangles right after
//    generation).
//
// 2. The second version kept that save call but then called World->DestroyActor() on each saved
//    actor to free memory. This was proven, via empirical control tests in the editor, to be
//    unrecoverable data loss: World Partition treats a destroyed actor as permanently deleted, not
//    "unloaded". Root cause (confirmed against UE 5.8 engine source): UWorld::DestroyActor
//    (LevelActor.cpp) calls GEngine->BroadcastLevelActorDeleted(), which
//    UWorldPartition::OnLevelActorDeleted (WorldPartition.cpp) handles by fully removing the
//    actor's FWorldPartitionActorDescInstance descriptor (ActorDescContainerInstance.cpp) --  not
//    unloading it, deleting WP's own record that the content exists -- plus MarkAsGarbage() /
//    MarkPackageDirty() on the actor. That's indistinguishable from a user pressing Delete in the
//    World Outliner, so the next save (ours or an unrelated manual Ctrl+S) cleans up the "orphaned"
//    package. A genuinely non-destructive unload mechanism does exist (FLoaderAdapterActorList,
//    used by production UWorldPartitionBuilder code and the WP editor's own "Loaded Regions" UI --
//    its Unload() only decrements a FWorldPartitionReference and never calls DestroyActor) but
//    every confirmed engine caller operates on actors already loaded *through* that
//    reference-counted system; our actors are freshly SpawnActor'd in the same session and never go
//    through that load path, so mixing them in has no verified engine precedent and was not risked.
//
// SaveAndReloadLevel below instead frees memory by closing and reopening the level's own file
// (FEditorFileUtils::SaveCurrentLevel() then FEditorFileUtils::LoadMap()) -- a wholesale
// teardown/reinstantiation, not a deletion, so already-saved external actor packages are untouched
// on disk while everything currently resident gets freed. Heavier than a targeted per-actor unload,
// but every mechanic involved is confirmed, source-verified, standard editor machinery (this is
// literally what "File > Open Level" does).
class BUILDINGGRAMMARRUNTIME_API FBuildingActorPersistence
{
public:
	// Whether World is World-Partition-enabled -- a hard precondition for the save/reload flow
	// below. Without WP, an "external" actor package has nothing to hash/stream it back in.
	static bool IsWorldPartitioned(const UWorld* World);

	// Saves every actor's external (OFPA) package (AActor::SetPackageExternal, then a batched
	// FEditorFileUtils::PromptForCheckoutAndSave call -- no UI prompt, runs headless). Does NOT
	// touch residency -- actors stay fully live/usable in their World afterward. See
	// SaveAndReloadLevel for the actual memory-freeing step; the two are separate because a caller
	// may want to batch several saves before paying for one (much more expensive) reload. Every
	// actor's RuntimeGrid (see ABuildingInstancePoolActor::SetBuildingRuntimeGrid) must already be
	// set before calling this -- World Partition captures it at the moment of this save, not
	// afterward. Returns the number of actors successfully saved; actors whose package failed to
	// save are appended to OutFailedActors (left dirty/resident, not lost).
	static int32 SaveActors(const TArray<AActor*>& Actors, TArray<AActor*>& OutFailedActors);

	// Headless save of the current level's own package (map package plus any dirty/new OFPA
	// external actor packages) -- FEditorFileUtils::SaveCurrentLevel(). Returns false if the save
	// failed.
	static bool SaveLevel();

	// Frees memory by saving and then closing/reopening World's own level file -- NOT by destroying
	// individual actors (see this class's header comment for why that's unsafe). Calls SaveLevel()
	// first, then reloads via FEditorFileUtils::LoadMap(). This frees every actor currently
	// resident -- including everything generated and saved so far this run -- without deleting
	// anything already on disk. World is reassigned to the freshly loaded world; every other
	// UWorld*/AActor* the caller was holding from before this call is now stale and must not be used
	// again. Returns false (World left unchanged, or null if the reload itself failed partway) if
	// the level has never been saved to disk before (nothing to reload from) or the save/reload
	// failed.
	static bool SaveAndReloadLevel(UWorld*& World);
};
