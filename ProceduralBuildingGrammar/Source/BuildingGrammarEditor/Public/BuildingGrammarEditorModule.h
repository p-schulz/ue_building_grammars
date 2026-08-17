#pragma once

#include "Modules/ModuleManager.h"
#include "Config/BuildingGrammarConfig.h"

class ABuildingInstancePoolActor;
class IStructureDetailsView;
class SWindow;
class UWorld;
class FStructOnScope;
struct FPropertyChangedEvent;
struct FCanLoadMap;

// Registers entries under "Tools > Procedural Building Grammar" in the Level Editor main menu:
// loading a preset config from a snake_case JSON file (FGrammarConfigJson -- see its header for
// why that's a different format from FBuildingGrammarConfig's own JSON round-trip), generating
// buildings from an .osm file using whichever config was most recently loaded that way (falling
// back to the built-in urban_block preset if none has been), baking generated cells to static
// meshes, deleting all generated building pool actors (a teardown workaround -- see
// OnDeleteAllBuildingPoolsClicked's own comment), importing trees from a GeoJSON tree-cadastre
// export filtered to a loaded OSM file's own region (see OnImportTreesFromGeoJsonClicked), and
// toggling the "Pick Building" viewport tool (FBuildingPickEdMode) for post-import per-building
// customization. See BuildingGrammarEditorModule.cpp for all flows.
class FBuildingGrammarEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OnLoadConfigFromJsonClicked();

	// Explicit counterpart to the automatic "first import in a level establishes it" behavior every
	// OSM-driven generate/import action now has (AGeoReferenceOriginActor::ResolveOrigin) -- lets a
	// user pin the shared origin to a specific file's bounds BEFORE importing anything (rather than
	// whatever the first import happens to be), or re-anchor deliberately later. File-pick + bounds-
	// center derivation identical to OnGenerateFromOsmClicked's own origin step; warns first if a
	// reference already exists, since changing it misaligns anything already generated against the
	// old one.
	void OnSetLevelGeoReferenceClicked();

	// Deletes the level's AGeoReferenceOriginActor if one exists (after a confirmation dialog),
	// so the next OSM import re-establishes a fresh one from its own file -- e.g. to start an
	// unrelated area in the same level without stitching it to whatever was imported before.
	void OnClearLevelGeoReferenceClicked();

	void OnGenerateFromOsmClicked();
	void OnBakeToStaticMeshClicked();
	void OnPickBuildingClicked();

	// Deletes every ABuildingInstancePoolActor in the current editor world (after a confirmation
	// dialog showing the count). Workaround for a known issue: closing/reopening a level (or the
	// editor) with many live pool actors -- each owning a UDynamicMeshComponent hero mesh plus a
	// UHierarchicalInstancedStaticMeshComponent per (Role, VariantKey) bucket -- can leave the
	// editor window unresponsive during teardown; deleting them first via this action avoids that.
	void OnDeleteAllBuildingPoolsClicked();

	// Automatic counterpart to OnDeleteAllBuildingPoolsClicked -- bound to
	// FEditorDelegates::OnMapLoad, which FEditorFileUtils::LoadMap broadcasts as the very first
	// thing it does, before the current level's teardown begins (verified against engine source,
	// not assumed) -- covers both "Open Level" and "New Level" (the latter's
	// UEditorLoadingAndSavingUtils::NewMapFromTemplate routes through LoadMap too, so it fires for
	// that as well). Deletes silently (log only, no confirmation dialog -- an automatic hook
	// shouldn't block on a modal every time a level is opened) so the expensive teardown that
	// otherwise makes the editor unresponsive never has anything left to tear down. Does NOT cover
	// closing the editor itself -- see this module's own investigation notes (the conversation that
	// added this, not reproduced in-source) for why FCoreDelegates::OnEnginePreExit turned out to
	// fire too late for that case (UEditorEngine::PreExit already calls World->CleanupWorld()
	// before broadcasting it).
	void HandleMapLoad(const FString& Filename, FCanLoadMap& OutCanLoadMap);

	// Shared by OnDeleteAllBuildingPoolsClicked and HandleMapLoad -- finds and destroys every
	// ABuildingInstancePoolActor in World, returning how many were destroyed. No UI of its own;
	// callers own confirmation/reporting.
	static int32 DeleteAllBuildingPools(UWorld* World);

	// Alternative to OnGenerateFromOsmClicked: drives the BuildingGrammarPCG module's PCG-graph-based
	// pipeline instead of the deterministic C++ engine. Reuses the same file-picker UX; sets the
	// picked file as the target UPCGComponent's "OsmFilePath" Graph Parameter and triggers Generate.
	void OnGeneratePCGClicked();

	// Prompts for a GeoJSON tree file, then an .osm file (used ONLY to derive the projection origin
	// -- the same FOsmDocument::GetBoundsCenter derivation OnGenerateFromOsmClicked uses, so trees
	// line up with buildings generated from the same file -- and the region to filter trees into,
	// via its own bounds), and calls
	// UTreeImportLibrary::ImportTreesFromGeoJson to spawn one ATreeInstancePoolActor. See that
	// function's own header comment for the filtering/projection/ground-snap/random-rotation
	// details; see UTreeMeshSettings (Project Settings > Plugins > Procedural Building Grammar -
	// Trees) for assigning meshes per tree type -- a type with none assigned yet is silently skipped
	// rather than spawned with a placeholder.
	void OnImportTreesFromGeoJsonClicked();

	// Bound to FBuildingPickEdMode::OnBuildingPicked -- shows/refreshes the floating customization
	// details panel for the just-picked building (see BuildingPickPanelData.h).
	void HandleBuildingPicked(ABuildingInstancePoolActor* Pool, const FString& SourceName);

	// Bound to the details panel's OnFinishedChangingProperties -- applies whatever the panel's
	// current struct memory holds (PickPanelStruct) to PickedPool and regenerates its cell. No
	// separate "Apply" button; edits take effect as soon as a field commits.
	void HandlePickPanelPropertyChanged(const FPropertyChangedEvent& ChangedEvent);

	TOptional<FBuildingGrammarConfig> LoadedConfig;

	// Reused across picks rather than recreated -- see this module's .cpp for why (avoids
	// FGlobalTabmanager registration complexity for what's a single, occasional-use tool). Reset to
	// null (and recreated on the next pick) if the user closes the window via its native close button.
	TSharedPtr<FStructOnScope> PickPanelStruct;
	TSharedPtr<IStructureDetailsView> PickPanelDetailsView;
	TSharedPtr<SWindow> PickPanelWindow;
	TWeakObjectPtr<ABuildingInstancePoolActor> PickedPool;
};
