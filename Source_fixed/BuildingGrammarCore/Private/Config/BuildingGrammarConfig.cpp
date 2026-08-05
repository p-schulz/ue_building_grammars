#include "Config/BuildingGrammarConfig.h"
#include "JsonObjectConverter.h"

TArray<FString> FBuildingGrammarConfig::DefaultBatchRoles()
{
	return {
		TEXT("window"), TEXT("window_frame"), TEXT("window_mullion"), TEXT("window_sill"),
		TEXT("ledge"),
		TEXT("balcony"), TEXT("balcony_rail"), TEXT("balcony_bar"),
		TEXT("door_frame"), TEXT("door_handle"), TEXT("door_canopy"),
		TEXT("signboard"), TEXT("awning"), TEXT("garage_door"), TEXT("loading_dock"), TEXT("stair_core"), TEXT("shutter"),
		TEXT("facade_ornament"), TEXT("panel_seam"), TEXT("insulation_band"),
		TEXT("roof_edge"), TEXT("roof_lamp"), TEXT("roof_tile"), TEXT("roof_window"),
		TEXT("dormer"), TEXT("chimney"), TEXT("gutter"),
		TEXT("pv_panel"), TEXT("hvac_unit"), TEXT("roof_plant"),
		TEXT("antenna"), TEXT("antenna_panel")
	};
}

FString FBuildingGrammarConfig::ToJsonString(const FBuildingGrammarConfig& Config)
{
	FString JsonString;
	FJsonObjectConverter::UStructToJsonObjectString(Config, JsonString);
	return JsonString;
}

bool FBuildingGrammarConfig::FromJsonString(const FString& JsonString, FBuildingGrammarConfig& OutConfig)
{
	return FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &OutConfig, 0, 0);
}
