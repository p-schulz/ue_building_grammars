#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGOrientToNearestStreet.generated.h"

UENUM(BlueprintType)
enum class EGrammarStreetAlignmentMode : uint8
{
	// Local +X faces along the nearest street's own tangent direction -- e.g. streetlights/benches.
	Parallel,
	// Local +X faces across the nearest street, perpendicular to its tangent -- e.g. a sign facing
	// oncoming traffic.
	Perpendicular,
};

// Layout PCG node: a pure Transform-modifying pass -- takes any point data on its "Points" pin (e.g.
// UPCGLoadOsmPointFeaturesSettings' "Features" pin, or
// UPCGPlaceStreetLightsAlongLitRoadsSettings' "Lights" pin) and its "Streets" pin (UPCGSplineData,
// matching UPCGGetStreetNetworkSettings' own output), and rotates every point to face the nearest
// street -- position and every one of that point's existing attributes pass through unchanged. Kept
// as a separate, composable node rather than baked into any one point-source node, since not every
// point-source needs street orientation and this same node works for all of them.
//
// Per point: builds a TArray<FStreetSegment> from the Streets pin (same gather loop
// UPCGFacadeWindowDoorLayoutSettings/UPCGRoofFrameGeneratorSettings already have -- copied rather
// than shared, per this module's own established precedent of small per-node duplication for this
// exact loop), then calls the existing FindNearestStreetDirection(Footprint, Candidates,
// SearchRadius, OutDirection) (PCGBuildingGrammarDefaults.h) with a single-point "footprint" (that
// point's own location -- its centroid IS the point) to get the nearest street's 2D tangent. Builds
// the final rotation via FRotationMatrix::MakeFromXZ(Forward, FVector::UpVector) -- explicit
// up-vector, NOT MakeFromXY, since MakeFromXY's Z axis is computed and can end up pointing
// world-down for certain windings (see UPCGExtrudeFootprintToWallsSettings' own documented sign
// gotcha); these are upright objects, so an explicit up-vector avoids that entirely. A point with no
// street within SearchRadius keeps its existing rotation unchanged (not reset to identity --
// "couldn't find a street" shouldn't erase whatever rotation the point already had).
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGOrientToNearestStreetSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("OrientToNearestStreet")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGOrientToNearestStreetSettings", "NodeTitle", "Orient To Nearest Street"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGOrientToNearestStreetSettings", "NodeTooltip", "Rotates each input point to face the nearest street, parallel or perpendicular to it -- position and every other attribute pass through unchanged."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EGrammarStreetAlignmentMode AlignmentMode = EGrammarStreetAlignmentMode::Parallel;

	// Unreal centimeters -- a point farther than this from every street keeps its existing rotation
	// unchanged.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0"))
	double SearchRadius = 3000.0;
};

class FPCGOrientToNearestStreetElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
