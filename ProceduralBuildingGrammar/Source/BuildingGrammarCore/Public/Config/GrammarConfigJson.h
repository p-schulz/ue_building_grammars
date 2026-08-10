#pragma once

#include "CoreMinimal.h"
#include "Config/BuildingGrammarConfig.h"

class FJsonObject;
class FJsonValue;

// Reads and writes the Blender add-on's own JSON schema exactly -- config.py's
// BuildingGrammarConfig.from_dict/to_dict and every nested _x_from_dict/asdict() it calls,
// snake_case field names and all (e.g. "wall_material", "default_floor_height",
// "irregular_floor_heights": {"0": 4.1}). This is deliberately a *different* format from
// FBuildingGrammarConfig::ToJsonString/FromJsonString (which round-trips this plugin's own
// PascalCase UPROPERTY-name JSON via FJsonObjectConverter) -- that format is self-consistent but
// can't read anything the Python add-on ever produced. This one can: point
// LoadConfigFromPythonJsonFile at the bundled german_building_grammar_config.json (or any config
// exported from the Blender add-on's "Save Config" operator) and get a fully-populated
// FBuildingGrammarConfig, including every style, without hand-porting a single field.
//
// Root-level fields the Python schema has that this plugin's FBuildingGrammarConfig deliberately
// doesn't (root_collection, source_collection, include_selected_only, replace_existing -- see
// BuildingGrammarConfig.h's comment) are read-and-discarded on import, and simply omitted on
// export; they round-trip through the Blender add-on fine on their own defaults, they just aren't
// something this plugin has an opinion about.
class BUILDINGGRAMMARCORE_API FGrammarConfigJson
{
public:
	static bool LoadConfigFromPythonJsonFile(const FString& FilePath, FBuildingGrammarConfig& OutConfig, FString& OutError);
	static bool LoadConfigFromPythonJsonString(const FString& JsonString, FBuildingGrammarConfig& OutConfig, FString& OutError);

	// Pretty-printed, matching the Blender add-on's own indented json.dump output.
	static FString SaveConfigToPythonJsonString(const FBuildingGrammarConfig& Config);
	static bool SaveConfigToPythonJsonFile(const FString& FilePath, const FBuildingGrammarConfig& Config, FString& OutError);

private:
	static FBuildingGrammarConfig ConfigFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject);
	static FFacadeStyleConfig StyleFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject);
	static FWindowStyleConfig WindowFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject);
	static FLedgeStyleConfig LedgeFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject);
	static FBalconyStyleConfig BalconyFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject);
	static FDoorStyleConfig DoorFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject);
	static FAntennaStyleConfig AntennaFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject);
	static FRoofStyleConfig RoofFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject);

	static TSharedRef<FJsonObject> ConfigToJsonObject(const FBuildingGrammarConfig& Config);
	static TSharedRef<FJsonObject> StyleToJsonObject(const FFacadeStyleConfig& Style);
	static TSharedRef<FJsonObject> WindowToJsonObject(const FWindowStyleConfig& Window);
	static TSharedRef<FJsonObject> LedgeToJsonObject(const FLedgeStyleConfig& Ledge);
	static TSharedRef<FJsonObject> BalconyToJsonObject(const FBalconyStyleConfig& Balcony);
	static TSharedRef<FJsonObject> DoorToJsonObject(const FDoorStyleConfig& Door);
	static TSharedRef<FJsonObject> AntennaToJsonObject(const FAntennaStyleConfig& Antenna);
	static TSharedRef<FJsonObject> RoofToJsonObject(const FRoofStyleConfig& Roof);
	// Low-level field helpers (mirroring config.py's _color/_color_list/_string_list) are internal
	// linkage in the .cpp, not class members -- nothing outside this file needs them.
};
