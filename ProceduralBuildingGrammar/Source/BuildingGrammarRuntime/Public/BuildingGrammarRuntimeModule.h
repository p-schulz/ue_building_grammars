#pragma once

#include "Modules/ModuleManager.h"

class FBuildingGrammarRuntimeModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
