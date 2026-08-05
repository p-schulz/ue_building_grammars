#pragma once

#include "Modules/ModuleManager.h"

class FBuildingGrammarGeometryModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
