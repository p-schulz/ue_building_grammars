#pragma once

#include "CoreMinimal.h"
#include "Config/FacadeStyleConfig.h"
#include "BuildingGrammarConfig.generated.h"

// Port of config.py's BuildingGrammarConfig -- the root generation-rule config. Deliberately does
// NOT carry Blender-specific scene-organization fields (root_collection, source_collection,
// include_selected_only, replace_existing): those describe *where in a Blender scene* to look for
// footprints and file results, which has no equivalent here -- the UE5 Editor tool and runtime
// streaming subsystem each take their own target (level/data layer/actor) as a separate parameter
// to the generation call rather than folding it into this rule config.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FBuildingGrammarConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Parts")
	bool bEnableBuildingParts = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Parts", meta = (EditCondition = "bEnableBuildingParts"))
	bool bSkipParentFootprintsWithParts = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Parts", meta = (EditCondition = "bEnableBuildingParts"))
	bool bInheritParentTagsForParts = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Parts", meta = (EditCondition = "bEnableBuildingParts"))
	double BuildingPartMatchTolerance = 0.25;

	// Drives the FPlacementRecord/HISM-pool instancing path (see BuildingGrammarRuntime) for every
	// role in BatchRoles, instead of building unique per-instance geometry for them.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	bool bUseMeshInstancing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	bool bBatchGeneratedMeshes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (EditCondition = "bUseMeshInstancing || bBatchGeneratedMeshes"))
	TArray<FString> BatchRoles = DefaultBatchRoles();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Levels")
	int32 DefaultLevels = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Levels")
	double DefaultFloorHeight = 3.1;

	// Floor index -> explicit floor height override, for buildings with an irregular ground/attic
	// floor height that isn't derivable from an explicit OSM height tag alone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Levels")
	TMap<int32, double> IrregularFloorHeights;

	// OSM building=* values that are skipped entirely unless a style explicitly matches them.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filtering")
	TArray<FString> ExcludedBuildingValues = { TEXT("shelter") };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styles")
	TArray<FFacadeStyleConfig> Styles = { FFacadeStyleConfig() };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styles")
	FRoofStyleConfig Roof;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roof")
	double RoofStreetAlignmentSearchRadius = 80.0;

	// The 28 roles eligible for HISM-pool instancing/mesh-role-merging by default -- every role
	// except the per-building-unique "hero" surfaces (facade, roof). Mirrors config.py's
	// DEFAULT_BATCH_ROLES exactly.
	static TArray<FString> DefaultBatchRoles();

	static FString ToJsonString(const FBuildingGrammarConfig& Config);
	static bool FromJsonString(const FString& JsonString, FBuildingGrammarConfig& OutConfig);
};
