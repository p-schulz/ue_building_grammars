#pragma once

#include "CoreMinimal.h"
#include "TreeTypes.generated.h"

// Coarse tree categorization matching what's actually present in typical German municipal
// open-data tree exports (e.g. a city's own "Baumkataster" GeoJSON) -- FGeoJsonTreeParser maps
// its baumart_allgemein field onto this enum. Deliberately coarse rather than a full species list:
// species-level fields (artdeut/artlat) are frequently null in practice, sometimes on every single
// feature in an export. Add more values here (and to FGeoJsonTreeParser::ParseTreeType) if a
// richer dataset becomes available -- this is the enumerable "list of available tree types" that
// UTreeMeshSettings (BuildingGrammarRuntime) resolves a mesh set for.
UENUM(BlueprintType)
enum class EGrammarTreeType : uint8
{
	FruitTree UMETA(DisplayName = "Fruit Tree (Obstbaum)"),
	Broadleaf UMETA(DisplayName = "Broadleaf (Laubbaum)"),
	Unknown UMETA(DisplayName = "Unknown (unbekannt)")
};

// One parsed tree point in raw (unprojected) WGS84 degrees -- see FGeoJsonTreeParser. Plain C++
// struct (not USTRUCT/reflected), same as FOsmNode/FOsmWay (Osm/OsmTypes.h): an internal parse
// intermediate, never exposed to Blueprint or serialized to a DataAsset on its own.
struct BUILDINGGRAMMARCORE_API FGrammarTreePoint
{
	double Latitude = 0.0;
	double Longitude = 0.0;
	EGrammarTreeType Type = EGrammarTreeType::Unknown;
};
