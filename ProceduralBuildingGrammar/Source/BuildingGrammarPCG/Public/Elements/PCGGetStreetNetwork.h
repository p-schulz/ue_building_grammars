#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGGetStreetNetwork.generated.h"

// Data-source PCG node: parses an .osm file's highway=* ways into street polylines, reusing
// FStreetNetworkAssembler (BuildingGrammarCore) and the same FLocalTangentPlaneProjection origin
// convention as UPCGLoadOsmBuildingVolumesSettings, so a building-volumes node and a street-network
// node fed the same file/origin land in exactly the same coordinate space. This is data ingestion,
// not generation layout -- see the module's own header comment.
//
// Outputs two pins, index-aligned by street way:
//  - "Streets": one open UPCGSplineData per street way (in UE centimeters, same X=local-North/
//    Y=local-East convention as the rest of this plugin), tagged with "Name:<value>" if the way has
//    a `name` tag.
//  - "StreetInfo": a single UPCGParamData attribute set, one row per street (keyed by way index),
//    with a Name column (empty for unnamed ways -- FGrammarStreetSegment/FStreetNetworkAssembler
//    doesn't currently carry the raw highway=* value, only the extraction rule it implies).
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGGetStreetNetworkSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetStreetNetwork")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGGetStreetNetworkSettings", "NodeTitle", "Get Street Network"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGGetStreetNetworkSettings", "NodeTooltip", "Parses an .osm file's highway=* ways into street polylines, output as spline geometry plus a parallel attribute set of street metadata."); }
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

class FPCGGetStreetNetworkElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
