#pragma once

#include "Modules/ModuleManager.h"

// Registers "Tools > Procedural Building Grammar > Generate Buildings from OSM..." in the Level
// Editor main menu. See BuildingGrammarEditorModule.cpp for the generation flow itself.
class FBuildingGrammarEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OnGenerateFromOsmClicked();
};
