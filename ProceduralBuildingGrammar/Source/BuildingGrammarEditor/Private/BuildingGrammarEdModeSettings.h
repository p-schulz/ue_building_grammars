#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "Osm/FlexOsmImportSettings.h"
#include "Config/BuildingGrammarConfig.h"
#include "Config/RoofStyleConfig.h"
#include "Parcel/GrammarParcelTypes.h"
#include "BuildingGrammarEdModeSettings.generated.h"

class UOsmDataAsset;
class UWorld;

/** Which viewport interaction the editor mode's clicks/drags currently drive -- see
 * FBuildingPickEdMode::HandleClick/InputKey/InputDelta, which all dispatch on this. */
UENUM()
enum class EBuildingGrammarEditTool : uint8
{
	/** Click a generated building to open its Building Customization panel (tags/forced style) -- the mode's original behavior. */
	Select,
	/** Click-click to place footprint corners connected by straight edges; click near the first corner (or press Enter) to close the loop and generate a building. */
	Place,
	/** Drag an existing hand-placed building's footprint corners to reshape it. */
	Move,
	/** Click inside a road-network block (from the most recent "Generate Buildings From Road
	 * Network" run -- see UBuildingGrammarEdModeSettings::LastParcelDebugData) to open a panel for
	 * tweaking that one block's parcel-subdivision method/config and regenerating just its buildings. */
	Block
};

/** Transient settings and commands displayed by the Building Grammar editor mode toolkit. */
UCLASS(Transient)
class UBuildingGrammarEdModeSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Which viewport tool is currently active -- see EBuildingGrammarEditTool. */
	UPROPERTY(EditAnywhere, Category = "Tool")
	EBuildingGrammarEditTool ActiveTool = EBuildingGrammarEditTool::Select;

	/** Facade style newly Place-tool-drawn buildings are generated with. Empty ("Auto") uses the
	 * config's normal tag-based style selection instead of forcing one -- see
	 * FBuildingCustomizationOverride::ForcedStyleName, which this feeds into. Populated from whichever
	 * config GrammarConfigFile/LoadGrammarConfig most recently resolved. */
	UPROPERTY(EditAnywhere, Category = "Tool|Place", meta = (GetOptions = "GetStyleNameOptions"))
	FString ActiveStyleName;

	UFUNCTION()
	TArray<FString> GetStyleNameOptions() const;

	/** Overrides the number of floors newly Place-tool-drawn buildings are generated with -- feeds
	 * FBuildingCustomizationOverride::bOverrideLevels/Levels on the new building, the same field the
	 * Building Customization panel's Levels override writes to (see BuildingInstancePoolActor.h). */
	UPROPERTY(EditAnywhere, Category = "Tool|Place")
	bool bOverrideLevels = false;

	UPROPERTY(EditAnywhere, Category = "Tool|Place", meta = (EditCondition = "bOverrideLevels", ClampMin = "1"))
	int32 Levels = 4;

	/** Overrides the roof shape newly Place-tool-drawn buildings are generated with -- feeds
	 * FBuildingCustomizationOverride::bOverrideRoofType/RoofType on the new building. */
	UPROPERTY(EditAnywhere, Category = "Tool|Place")
	bool bOverrideRoofType = false;

	UPROPERTY(EditAnywhere, Category = "Tool|Place", meta = (EditCondition = "bOverrideRoofType"))
	EGrammarRoofType RoofType = EGrammarRoofType::Flat;

	/** Snaps each new Place-tool edge's direction (from the previously placed corner to the one about
	 * to be placed) to 15-degree increments measured from world +X -- same behavior/increment as
	 * FlexNetwork's own road-drawing angle snap (FFlexNetworkEdMode::ApplyAngleSnap). Has no effect on
	 * a draft's very first corner, since there's no previous corner yet to measure an edge from. */
	UPROPERTY(EditAnywhere, Category = "Tool|Place", meta = (DisplayName = "Snap Edges to 15°"))
	bool bAngleSnapEnabled = false;

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

	/** How each extracted road-network block is carved into building lots -- see
	 * EGrammarParcelSubdivisionMethod's own comments for what each option actually does. */
	UPROPERTY(EditAnywhere, Category = "Parcels")
	EGrammarParcelSubdivisionMethod ParcelSubdivisionMethod = EGrammarParcelSubdivisionMethod::Hybrid;

	UPROPERTY(EditAnywhere, Category = "Parcels", meta = (ShowOnlyInnerProperties))
	FGrammarParcelConfig ParcelConfig;

	/** Draws each block's boundary, the final parcel outlines (color-coded: green = buildable,
	 * orange = buildable with a constraint warning, red = rejected/no street access, purple = patio),
	 * and the subdivision algorithm's own debug rays/boxes (OBB split cuts, skeleton frontage rays,
	 * inner offset contours) in the viewport -- see FBuildingPickEdMode::Render. Populated by the most
	 * recent "Generate Buildings From Road Network" run; stays empty until that's been run at least
	 * once. */
	UPROPERTY(EditAnywhere, Category = "Parcels|Debug")
	bool bShowParcelDebugVisualization = false;

	/** Captured by the most recent "Generate Buildings From Road Network" run -- see
	 * bShowParcelDebugVisualization. Not a UPROPERTY: purely consumed by Render(), not meant to be
	 * edited or shown as a wall of numbers in the details panel. */
	TArray<FGrammarBlockDebugData> LastParcelDebugData;

	/** Extracts every closed block from the current level's FlexNetwork road graph
	 * (FFlexRoadBlockExtraction), subdivides each into parcels (ParcelSubdivisionMethod/ParcelConfig),
	 * and generates a building on every street-facing parcel using whichever config
	 * GrammarConfigFile/LoadGrammarConfig most recently resolved -- the same style-selection path
	 * "Generate Buildings From OSM Asset" uses. Respects bReplaceExistingBuildingPools. */
	UFUNCTION(CallInEditor, Category = "Parcels", meta = (DisplayName = "Generate Buildings From Road Network"))
	void GenerateBuildingsFromRoadNetwork();

	/** Refreshed by the mode so CallInEditor commands always operate on the current editor world. */
	TWeakObjectPtr<UWorld> TargetWorld;

	/** Lazily resolves (and caches) the config from GrammarConfigFile -- used by the Place tool's
	 * style dropdown (GetStyleNameOptions) and to seed a newly-created hand-placed pool's
	 * SourceConfig. Re-resolved by LoadGrammarConfig(); otherwise cached across calls so placing
	 * several buildings in a row doesn't re-parse the JSON file on every click. */
	const FBuildingGrammarConfig& GetResolvedConfigForPlacement() const;

private:
	bool ResolveConfig(FBuildingGrammarConfig& OutConfig, FString& OutError) const;

	mutable FBuildingGrammarConfig CachedPlacementConfig;
	mutable bool bCachedPlacementConfigValid = false;
};
