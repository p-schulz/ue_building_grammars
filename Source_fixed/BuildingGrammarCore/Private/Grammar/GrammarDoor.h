#pragma once

#include "CoreMinimal.h"
#include "Config/FacadeStyleConfig.h"
#include "Spec/PlacementRecord.h"

// Port of grammar.py's _door_mesh / _door_detail_meshes. All door elements (leaf, frame, handle,
// canopy) become FGrammarPlacementRecord -- see GrammarPlacementHelpers.h.
namespace GrammarDoor
{
	FGrammarPlacementRecord DoorPlacement(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double GroundFloorHeight, const FFacadeStyleConfig& Style);
	TArray<FGrammarPlacementRecord> DoorDetailPlacements(const FVector2D& Start, const FVector2D& End, const FVector2D& Normal, double Offset, double GroundFloorHeight, const FFacadeStyleConfig& Style);
}
