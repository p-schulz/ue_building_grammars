#pragma once

#include "CoreMinimal.h"
#include "Config/RoofStyleConfig.h"
#include "Spec/MeshSpec.h"

// Port of grammar.py's _roof_mesh dispatch + _gabled_roof_mesh/_hipped_roof_mesh/_pyramid_roof_mesh.
// The roof plane is hero geometry (per-building-unique) -- unlike every other roof-related role
// (tiles, dormers, roof windows, chimneys, gutters, edge trim, antennas -- see GrammarRoofDetails),
// so this produces a real FGrammarMeshSpec, not a placement.
namespace GrammarRoof
{
	FGrammarMeshSpec RoofMesh(const FString& SourceName, const TArray<FVector2D>& Footprint, double Height, const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags);
}
