#pragma once

#include "CoreMinimal.h"
#include "Config/RoofStyleConfig.h"

// Port of grammar.py's ridge-direction resolution chain (_gabled_ridge_direction,
// _roof_orientation_direction, _direction_from_cardinal_or_angle, _ridge_direction_from_tags).
// Shared by both the roof-plane (GrammarRoof.cpp) and roof-detail (GrammarRoofDetails.cpp)
// generators, which each build their own FGrammarRoofFrame using the same resolved direction.
namespace GrammarRoofDirection
{
	// Explicit roof:orientation/roof:direction tag first; else (only for gabled roofs, and only
	// when RidgeAlignment is street-based) an explicit ridge-direction tag; else the footprint's
	// longest AABB axis.
	FVector2D GabledRidgeDirection(const TArray<FVector>& Base, const FRoofStyleConfig& Roof, const TMap<FString, FString>& Tags);

	// (0,0) if no usable roof:orientation/roof:direction tag is present.
	FVector2D RoofOrientationDirection(const TArray<FVector>& Base, const TMap<FString, FString>& Tags);

	FVector2D DirectionFromCardinalOrAngle(const FString& Value);

	FVector2D RidgeDirectionFromTags(const TMap<FString, FString>& Tags);
}
