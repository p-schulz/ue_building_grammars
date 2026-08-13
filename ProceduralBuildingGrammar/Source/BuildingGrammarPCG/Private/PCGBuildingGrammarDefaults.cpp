#include "PCGBuildingGrammarDefaults.h"
#include "Config/BuildingGrammarConfig.h"
#include "Config/GrammarConfigJson.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

bool LoadDefaultGermanBuildingGrammarConfig(FBuildingGrammarConfig& OutConfig)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ProceduralBuildingGrammar"));
	if (!Plugin.IsValid())
	{
		return false;
	}

	const FString FilePath = Plugin->GetContentDir() / TEXT("german_building_grammar_config.json");
	FString LoadError;
	if (!FGrammarConfigJson::LoadConfigFromPythonJsonFile(FilePath, OutConfig, LoadError))
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildingGrammarPCG: failed to load default style config '%s': %s"), *FilePath, *LoadError);
		return false;
	}

	return true;
}

FString ExtractSourceNameFromTags(const TSet<FString>& Tags)
{
	static const FString Prefix = TEXT("SourceName:");
	for (const FString& Tag : Tags)
	{
		if (Tag.StartsWith(Prefix))
		{
			return Tag.Mid(Prefix.Len());
		}
	}
	return FString();
}
