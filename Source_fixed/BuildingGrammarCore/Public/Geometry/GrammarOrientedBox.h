#pragma once

#include "CoreMinimal.h"
#include "Geometry/GrammarFace.h"

// Port of grammar.py's _oriented_box -- the single box-mesh generator used by nearly every detail
// mesh in the grammar engine (roof tiles, dormers, chimneys, gutters, edge trim, antennas, PV
// panels, HVAC units, window frames/sills, door frames/handles/canopies, balcony slabs/rails, ...).
class BUILDINGGRAMMARCORE_API FGrammarOrientedBox
{
public:
	// Center/Tangent/Normal are in the horizontal (X,Y) plane; Width runs along Tangent, Depth
	// along Normal, Height is vertical starting at Bottom. Reproduces the exact 8-vertex layout
	// and 6-face winding of _oriented_box: 4 bottom corners (lateral,outward) in the order
	// (-hw,-hd),(hw,-hd),(hw,hd),(-hw,hd) at Z=Bottom, duplicated at Z=Bottom+Height for the top,
	// with faces [0,1,2,3] bottom, [4,7,6,5] top, and 4 side quads walking the same corner order.
	// This exact ordering must be preserved so downstream normal computation matches the original
	// without needing extra flip logic.
	static void Build(
		const FVector2D& Center,
		const FVector2D& Tangent,
		const FVector2D& Normal,
		double Width,
		double Depth,
		double Height,
		double Bottom,
		TArray<FVector>& OutVertices,
		TArray<FGrammarFace>& OutFaces);
};
