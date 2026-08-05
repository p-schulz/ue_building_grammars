#pragma once

#include "Modules/ModuleManager.h"
#include "Config/BuildingGrammarConfig.h"

// Registers two entries under "Tools > Procedural Building Grammar" in the Level Editor main menu:
// loading a preset config from a snake_case JSON file (FGrammarConfigJson -- see its header for
// why that's a different format from FBuildingGrammarConfig's own JSON round-trip), and generating
// buildings from an .osm file using whichever config was most recently loaded that way (falling
// back to the built-in urban_block preset if none has been). See
// BuildingGrammarEditorModule.cpp for both flows.
class FBuildingGrammarEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OnLoadConfigFromJsonClicked();
	void OnGenerateFromOsmClicked();

	TOptional<FBuildingGrammarConfig> LoadedConfig;
};
