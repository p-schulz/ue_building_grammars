#pragma once

#include "CoreMinimal.h"
#include "Spec/MeshSpec.h"
#include "Spec/PlacementRecord.h"
#include "BuildingSpec.generated.h"

// Port of grammar.py's BuildingSpec -- the full output of generating one building (or one
// building:part volume) from a footprint + OSM tags + config. Split into HeroMeshes (unique
// per-building facade/roof geometry) and Placements (everything instanced via HISM pools); see
// FGrammarMeshSpec/FGrammarPlacementRecord for why the Python design's single flat `meshes` list
// is divided this way in the port.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarBuildingSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	FString SourceName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	int32 Levels = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	double Height = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	TArray<double> FloorHeights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	TArray<FGrammarMeshSpec> HeroMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	TArray<FGrammarPlacementRecord> Placements;

	TArray<const FGrammarMeshSpec*> HeroMeshesByRole(const FString& Role) const
	{
		TArray<const FGrammarMeshSpec*> Result;
		for (const FGrammarMeshSpec& Mesh : HeroMeshes)
		{
			if (Mesh.Role == Role)
			{
				Result.Add(&Mesh);
			}
		}
		return Result;
	}
};
