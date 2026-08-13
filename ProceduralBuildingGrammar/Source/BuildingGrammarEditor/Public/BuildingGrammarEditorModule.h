#pragma once

#include "Modules/ModuleManager.h"
#include "Config/BuildingGrammarConfig.h"

class ABuildingInstancePoolActor;
class IStructureDetailsView;
class SWindow;
class FStructOnScope;
struct FPropertyChangedEvent;

// Registers entries under "Tools > Procedural Building Grammar" in the Level Editor main menu:
// loading a preset config from a snake_case JSON file (FGrammarConfigJson -- see its header for
// why that's a different format from FBuildingGrammarConfig's own JSON round-trip), generating
// buildings from an .osm file using whichever config was most recently loaded that way (falling
// back to the built-in urban_block preset if none has been), baking generated cells to static
// meshes, and toggling the "Pick Building" viewport tool (FBuildingPickEdMode) for post-import
// per-building customization. See BuildingGrammarEditorModule.cpp for all flows.
class FBuildingGrammarEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OnLoadConfigFromJsonClicked();
	void OnGenerateFromOsmClicked();
	void OnBakeToStaticMeshClicked();
	void OnPickBuildingClicked();

	// Alternative to OnGenerateFromOsmClicked: drives the BuildingGrammarPCG module's PCG-graph-based
	// pipeline instead of the deterministic C++ engine. Reuses the same file-picker UX; sets the
	// picked file as the target UPCGComponent's "OsmFilePath" Graph Parameter and triggers Generate.
	void OnGeneratePCGClicked();

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
