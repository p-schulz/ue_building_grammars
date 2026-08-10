#pragma once

#include "CoreMinimal.h"
#include "Spec/PlacementRecord.h"

// Builds an FGrammarPlacementRecord for a role whose kit part is authored (see docs/PLAN.md
// section 4, Phase 4) as a 1x1x1-meter unit box: X = Tangent/width axis, Y = Normal/depth axis,
// Z = up/height axis, pivot at the box's center. Every grammar.py detail-mesh function that called
// _oriented_box for a role that isn't a hero surface (windows, frames, mullions, sills, ledges,
// balconies + rails/bars, door frame/handle/canopy, roof tiles/edge/windows, dormers, chimneys,
// gutters, antennas + accessories, PV/HVAC/plant clutter, shutters/signboards/awnings/garage doors,
// panel seams, insulation bands, ornament bands/pilasters, stair cores) becomes a call to
// MakeBoxPlacement instead of a real 8-vertex box: we only need the transform a shared unit-box
// kit mesh would need to reproduce the same box (Location = box center, Rotation = the
// Tangent/Normal/Up basis, Scale = (Width, Depth, Height)) -- building real per-instance geometry
// for these roles would be immediately thrown away in favor of the HISM-pool instancing described
// in the plan, so this port skips that step entirely rather than translating it faithfully only to
// discard it.
struct FGrammarBoxPlacementParams
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D Tangent = FVector2D(1.0, 0.0);
	FVector2D Normal = FVector2D(0.0, 1.0);
	double Width = 1.0;
	double Depth = 1.0;
	double Height = 1.0;
	double Bottom = 0.0;
};

class FGrammarPlacementHelpers
{
public:
	static FGrammarPlacementRecord MakeBoxPlacement(const FString& Role, const FString& VariantKey, const FGrammarBoxPlacementParams& Params, const FLinearColor& Color);
};
