#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGGetBarrierNetwork.generated.h"

// Data-source PCG node: parses an .osm file's barrier=wall/fence/guard_rail/hedge ways into linear
// barrier polylines, reusing FBarrierNetworkAssembler (BuildingGrammarCore) and the same
// FLocalTangentPlaneProjection origin convention as every other OSM-source node in this pipeline, so
// a barrier network node fed the same file/origin lands in exactly the same coordinate space as
// buildings/streets/point features.
//
// Outputs two pins, index-aligned by barrier way:
//  - "Barriers": one open UPCGSplineData per barrier way (in UE centimeters, same X=local-North/
//    Y=local-East convention as the rest of this plugin), tagged with "Type:<value>" (the raw
//    barrier=* tag value), "Height:<value>" (UE centimeters, only if the way has a parseable
//    `height` tag), and "Material:<value>" (only if the way has a `material` tag) -- same prefix-tag
//    convention UPCGGetStreetNetworkSettings already uses for "Name:<value>"/"Lit:<value>", so a
//    downstream consumer (UPCGExtrudeBarrierWaySettings) can read everything it needs directly off
//    this one pin without also wiring/correlating "BarrierInfo".
//  - "BarrierInfo": a single UPCGParamData attribute set, one row per barrier (keyed by way index),
//    with Type/Height/Material columns, for inspection or use outside the tag-reading convention
//    above.
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGGetBarrierNetworkSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetBarrierNetwork")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGGetBarrierNetworkSettings", "NodeTitle", "Get Barrier Network"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGGetBarrierNetworkSettings", "NodeTooltip", "Parses an .osm file's barrier=wall/fence/guard_rail/hedge ways into linear polylines, output as spline geometry plus a parallel attribute set of barrier metadata."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FFilePath OsmFilePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double OriginLatitude = 0.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double OriginLongitude = 0.0;
};

class FPCGGetBarrierNetworkElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
