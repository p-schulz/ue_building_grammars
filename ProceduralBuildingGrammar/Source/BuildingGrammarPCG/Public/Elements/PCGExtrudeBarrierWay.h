#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"

class UMaterialInterface;

#include "PCGExtrudeBarrierWay.generated.h"

// Layout PCG node: extrudes a barrier way's polyline (UPCGGetBarrierNetworkSettings' "Barriers"
// output) into a continuous vertical mesh strip, one quad per segment -- the same per-edge
// quad-append architecture UPCGExtrudeFootprintToWallsSettings uses for building walls, adapted for
// an OPEN polyline (Count-1 segments, no closing wrap) instead of a closed footprint ring, with no
// wall-overlap suppression (not applicable to a fence/wall line) and no window/door openings (calls
// the shared FGrammarWallRecess::BuildSegments with an empty Openings array, which still gets the
// correct single flush quad + winding + UV logic for free, reusing exported BuildingGrammarCore
// geometry rather than reimplementing quad math a third time).
//
// Height is read per-segment from the "Barriers" pin's own "Height:<value>" tag (see
// UPCGGetBarrierNetworkSettings' header comment) when present, falling back to this node's own
// Settings-level Height otherwise -- same Settings-level-fallback convention every other node in
// this pipeline already uses. Material is a single node-wide Settings value for this first pass
// (not resolved per-Type/per-OSM-`material`-tag -- that would need a general string-to-material
// lookup mechanism this codebase doesn't have yet, unlike FGrammarKitResolver's own per-STYLE
// resolution, which doesn't fit a barrier way at all); the "Barriers" pin's own "Type:<value>"/
// "Material:<value>" tags and "BarrierInfo" pin's matching columns are still available for a user to
// branch on manually (e.g. a Filter node splitting by Type before feeding separate Extrude Barrier
// Way nodes, each with its own Material) if per-type materials are needed.
//
// Outputs a single "Barrier" UPCGDynamicMeshData pin (unset material renders with the engine's
// default material, same as UPCGExtrudeFootprintToWallsSettings' own "Walls" pin).
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGExtrudeBarrierWaySettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("ExtrudeBarrierWay")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExtrudeBarrierWaySettings", "NodeTitle", "Extrude Barrier Way"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGExtrudeBarrierWaySettings", "NodeTooltip", "Extrudes a barrier way's polyline into a continuous vertical mesh strip -- walls, fences, and guardrails."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	// UE centimeters. Fallback whenever a barrier way has no parseable `height` tag.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Height = 180.0;

	// Assigned to material slot 0 of the output UPCGDynamicMeshData -- unset (the "Spawn Dynamic
	// Mesh" stock node's default) renders with the engine's default material, same as
	// UPCGExtrudeFootprintToWallsSettings' own fallback.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<UMaterialInterface> Material;

	// World-space UV tiling size in UE centimeters -- same convention as
	// UPCGExtrudeFootprintToWallsSettings' own TextureScale.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double TextureScale = 100.0;

	// Same independent winding/normal toggles as UPCGExtrudeFootprintToWallsSettings -- try both if
	// the barrier mesh appears invisible or renders visible-but-black.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bFlipWinding = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bFlipNormals = false;
};

class FPCGExtrudeBarrierWayElement : public IPCGElement
{
public:
	// Settings->Material.LoadSynchronous() -- same game-thread requirement as
	// UPCGExtrudeFootprintToWallsSettings' own identical override.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
