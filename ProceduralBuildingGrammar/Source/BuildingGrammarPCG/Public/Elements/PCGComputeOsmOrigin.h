#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGComputeOsmOrigin.generated.h"

// Data-source PCG node: computes a sensible FLocalTangentPlaneProjection origin for an .osm file --
// the midpoint of the file's own <bounds> element (or, for a file that lacks one, of every node's
// own Lat/Lon extent -- see FOsmDocument::GetBounds/GetBoundsCenter), reusing the exact same logic
// every other "generate/import" action in this plugin (and ProceduralRoads) uses to derive its
// origin automatically, rather than requiring it typed in by hand or computed differently by each
// caller. If the current level already has a geo reference (AGeoReferenceOriginActor -- set via
// "Set Level Geo Reference..." or established by an earlier import), that existing reference is used
// instead of this file's own bounds, so PCG-generated content stitches together with content from
// other OSM extracts imported into the same level over time rather than each one recentering on
// itself; if not, this file's own bounds-center becomes the level's new reference.
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
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGComputeOsmOriginSettings", "NodeTooltip", "Computes a projection origin (the .osm file's own <bounds> midpoint) so every node projecting that file shares the exact same origin."); }
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
