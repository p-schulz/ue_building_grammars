#pragma once

#include "CoreMinimal.h"
#include "GrammarFace.generated.h"

// One polygonal face as a CCW loop of indices into a mesh-spec's vertex array. Deliberately not
// pre-triangulated -- quads stay quads, roof fans/gables keep their natural vertex count -- so the
// exact face shapes from grammar.py's mesh builders (_oriented_box's quads, the gabled roof's
// quad sides, the pyramid roof's triangular fan, the hipped roof's mixed tri/quad faces) survive
// unchanged into BuildingGrammarGeometry, which triangulates when building the FDynamicMesh3.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarFace
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	TArray<int32> Indices;

	FGrammarFace() = default;
	explicit FGrammarFace(TArray<int32> InIndices) : Indices(MoveTemp(InIndices)) {}
};
