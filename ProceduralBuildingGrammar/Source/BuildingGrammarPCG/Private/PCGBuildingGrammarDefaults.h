#pragma once

#include "CoreMinimal.h"

struct FBuildingGrammarConfig;

// Loads the plugin's bundled Content/german_building_grammar_config.json -- the same file the
// classic (non-PCG) "Load Preset Config from JSON..." Tools-menu action loads on demand -- as the
// default config new PCG data-source/style nodes start with, so a user doesn't have to manually
// populate Config/Styles by hand or go through that menu action first before wiring up a PCG graph.
// Not module-exported (BUILDINGGRAMMARPCG_API): only used within this same module's own
// translation units. Returns false (OutConfig left untouched) if the plugin or the file can't be
// found/parsed -- callers should tolerate this (nodes must still construct successfully with a
// plain default config if the bundled file is ever missing/moved).
bool LoadDefaultGermanBuildingGrammarConfig(FBuildingGrammarConfig& OutConfig);

// Shared by every node that needs to correlate its own per-building input data (Footprint/Edges/
// Walls, all tagged "SourceName:<value>" by UPCGLoadOsmBuildingVolumesSettings and propagated
// downstream unchanged) with a StyleInfo row keyed by the same SourceName (UPCGSelectFacadeStyleSettings'
// UPCGParamData output, looked up via FindMetadataKey(FName(SourceName))). Returns an empty string
// (no match) if Tags carries no "SourceName:" entry.
FString ExtractSourceNameFromTags(const TSet<FString>& Tags);
