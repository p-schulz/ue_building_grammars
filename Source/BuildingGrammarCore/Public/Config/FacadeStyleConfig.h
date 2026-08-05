#pragma once

#include "CoreMinimal.h"
#include "Config/GrammarStringList.h"
#include "Config/WindowStyleConfig.h"
#include "Config/LedgeStyleConfig.h"
#include "Config/BalconyStyleConfig.h"
#include "Config/DoorStyleConfig.h"
#include "Config/AntennaStyleConfig.h"
#include "Config/RoofStyleConfig.h"
#include "FacadeStyleConfig.generated.h"

UENUM(BlueprintType)
enum class EGrammarWallColorVariantMode : uint8
{
	None UMETA(DisplayName = "None"),
	Cycle UMETA(DisplayName = "Cycle"),
	Building UMETA(DisplayName = "Building (stable hash)"),
	Facade UMETA(DisplayName = "Facade (stable hash)")
};

UENUM(BlueprintType)
enum class EGrammarWallRowColorMode : uint8
{
	Cycle UMETA(DisplayName = "Cycle"),
	GroundAccent UMETA(DisplayName = "Ground Accent")
};

// Port of config.py's FacadeStyleConfig. default_levels/default_floor_height/roof are Optional in
// Python (None = "fall back to the root BuildingGrammarConfig's value"); represented here as an
// explicit bHas*/value pair rather than a sentinel number, since UPROPERTY doesn't reflect
// TOptional<T> for arbitrary value types the way Python's None does.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FFacadeStyleConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade")
	FString Name = TEXT("default");

	// OSM building=*/building:part=*/building:use=* values this style matches (case-insensitive).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Matching")
	TArray<FString> BuildingValues;

	// Arbitrary OSM tag -> allowed-value-list matching, e.g. {"shop": ["*"]} or
	// {"amenity": ["place_of_worship"]}. "*" in the value list matches any non-empty tag value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Matching")
	TMap<FString, FGrammarStringList> TagFilters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Matching")
	bool bHasDefaultLevels = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Matching", meta = (EditCondition = "bHasDefaultLevels"))
	int32 DefaultLevels = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Matching")
	bool bHasDefaultFloorHeight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Matching", meta = (EditCondition = "bHasDefaultFloorHeight"))
	double DefaultFloorHeight = 3.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Roof")
	bool bOverrideRoof = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Roof", meta = (EditCondition = "bOverrideRoof"))
	FRoofStyleConfig RoofOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Wall")
	FString WallMaterial = TEXT("Grammar Facade");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Wall")
	FLinearColor WallColor = FLinearColor(0.72, 0.68, 0.6, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Wall")
	FString WallTexturePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Wall")
	double WallTextureScale = 1.0;

	// Per-side color variants, chosen by WallColorVariantMode; empty = always use WallColor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Wall Variants")
	TArray<FLinearColor> WallColorVariants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Wall Variants")
	EGrammarWallColorVariantMode WallColorVariantMode = EGrammarWallColorVariantMode::None;

	// Per-floor-row color variants (e.g. a distinct ground-floor accent color); empty = always use
	// WallColor. When non-empty, generation switches to one wall mesh per floor ("wall row") rather
	// than one mesh for the whole facade height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Wall Variants")
	TArray<FLinearColor> WallRowColors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade|Wall Variants")
	EGrammarWallRowColorMode WallRowColorMode = EGrammarWallRowColorMode::Cycle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade")
	FWindowStyleConfig Window;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade")
	FLedgeStyleConfig Ledge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade")
	FBalconyStyleConfig Balcony;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade")
	FDoorStyleConfig Door;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facade")
	FAntennaStyleConfig Antenna;
};
