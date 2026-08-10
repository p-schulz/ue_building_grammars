#pragma once

#include "Modules/ModuleManager.h"

class FBuildingGrammarCoreModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
