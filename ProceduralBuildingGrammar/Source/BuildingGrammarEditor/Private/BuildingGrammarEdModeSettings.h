#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "Osm/FlexOsmImportSettings.h"
#include "BuildingGrammarEdModeSettings.generated.h"

class UOsmDataAsset;
class UWorld;

/** Transient settings and commands displayed by the Building Grammar editor mode toolkit. */
UCLASS(Transient)
class UBuildingGrammarEdModeSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Generic OSM asset imported by FlexNetwork. Building ways and multipolygon relations are read from it. */
	UPROPERTY(EditAnywhere, Category = "OSM Asset")
	TObjectPtr<UOsmDataAsset> OsmAsset;

	/** Uses the same projection origin as FlexNetwork road generation for the selected asset. */
	UPROPERTY(EditAnywhere, Category = "OSM Asset", meta = (ShowOnlyInnerProperties))
	FFlexOsmImportSettings OsmImportSettings;

	/** Uses the level context published by FlexNetwork, ensuring buildings cannot use a different asset or origin accidentally. */
	UPROPERTY(EditAnywhere, Category = "OSM Asset|Shared Context")
	bool bUseFlexNetworkOsmContext = true;

	UFUNCTION(CallInEditor, Category = "OSM Asset|Shared Context", meta = (DisplayName = "Load FlexNetwork OSM Context"))
	void LoadFlexNetworkOsmContext();

	/** Explicitly replaces the level's shared context with the asset/settings shown above. */
	UFUNCTION(CallInEditor, Category = "OSM Asset|Shared Context", meta = (DisplayName = "Publish These Settings As Shared Context"))
	void PublishFlexNetworkOsmContext();

	/** Blender-schema BuildingGrammar JSON config. Leave empty to use the built-in Urban Block preset. */
	UPROPERTY(EditAnywhere, Category = "Building Grammar", meta = (FilePathFilter = "json"))
	FFilePath GrammarConfigFile;

	/** Generated pools are split into cells of this size to keep their bounds and instance buckets manageable. */
	UPROPERTY(EditAnywhere, Category = "Generation", meta = (ClampMin = "100.0", Units = "cm"))
	double CellSize = 10000.0;

	UPROPERTY(EditAnywhere, Category = "Generation")
	FName RuntimeGridName = NAME_None;

	/** Deletes existing live building pools before generation, preventing accidental duplicates. */
	UPROPERTY(EditAnywhere, Category = "Generation")
	bool bReplaceExistingBuildingPools = true;

	UPROPERTY(VisibleAnywhere, Category = "Status", meta = (MultiLine = true))
	FString LastResult = TEXT("Select a FlexNetwork OSM asset and generate buildings.");

	UFUNCTION(CallInEditor, Category = "Building Grammar", meta = (DisplayName = "Load / Validate Grammar Config"))
	void LoadGrammarConfig();

	UFUNCTION(CallInEditor, Category = "Generation", meta = (DisplayName = "Generate Buildings From OSM Asset"))
	void GenerateBuildingsFromOsmAsset();

	UFUNCTION(CallInEditor, Category = "Generation", meta = (DisplayName = "Delete Generated Building Pools"))
	void DeleteGeneratedBuildingPools();

	/** Refreshed by the mode so CallInEditor commands always operate on the current editor world. */
	TWeakObjectPtr<UWorld> TargetWorld;

private:
	bool ResolveConfig(struct FBuildingGrammarConfig& OutConfig, FString& OutError) const;
};
