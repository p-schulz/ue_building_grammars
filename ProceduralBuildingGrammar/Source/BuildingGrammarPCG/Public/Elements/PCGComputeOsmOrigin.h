#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGComputeOsmOrigin.generated.h"

// Data-source PCG node: computes a sensible FLocalTangentPlaneProjection origin for an .osm file --
// the center of its building footprints' bounding box (reusing
// FBuildingFootprintAssembler::ComputeFootprintBoundsCenter, the exact same logic the classic
// Tools-menu "Generate Buildings from OSM..." workflow already uses to derive its origin
// automatically, rather than requiring it typed in by hand).
//
// Wire this node's OsmFilePath to the SAME Graph Parameter as UPCGLoadOsmBuildingVolumesSettings and
// UPCGGetStreetNetworkSettings' OsmFilePath, then wire this node's "Origin" output into both of
// those nodes' OriginLatitude/OriginLongitude override pins (right-click/expose the pin on those
// PCG_Overridable properties, then connect a matching-named attribute from this node's Origin data).
// This guarantees all three nodes use exactly the same origin -- required for their outputs (building
// footprints and street polylines) to land in the same coordinate space -- without the origin ever
// needing to be looked up and typed in manually, and re-parses the file rather than threading a
// pre-parsed document through the graph, the same accepted small inefficiency
// FBuildingGrammarEditorModule::OnGenerateFromOsmClicked's own comment documents for the identical
// tradeoff in the non-PCG workflow.
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGComputeOsmOriginSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("ComputeOsmOrigin")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGComputeOsmOriginSettings", "NodeTitle", "Compute OSM Origin"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGComputeOsmOriginSettings", "NodeTooltip", "Computes a projection origin (building-footprint bounds center) for an .osm file, so every node projecting that file shares the exact same origin."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FFilePath OsmFilePath;
};

class FPCGComputeOsmOriginElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
