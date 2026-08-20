#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

class ABuildingInstancePoolActor;
class UBuildingGrammarEdModeSettings;
enum class EBuildingGrammarEditTool : uint8;

// Building Grammar editor mode. Its toolkit owns OSM-asset/config generation controls plus the
// active-tool/style-dropdown settings (see UBuildingGrammarEdModeSettings::ActiveTool). Three
// viewport tools share this one FEdMode, following FFlexNetworkEdMode's pattern of dispatching
// HandleClick/InputKey/InputDelta on a mode-settings enum rather than splitting into several FEdModes:
//   - Select (the mode's original behavior): HandleClick figures out which individual building
//     (identified by FGrammarBuildingVolume::SourceName) was actually clicked, out of the many merged
//     into a pool actor's shared HISM buckets/hero mesh, and broadcasts the result on a static
//     delegate rather than owning any UI itself -- FBuildingGrammarEditorModule owns showing/updating
//     the customization details panel (see that module's OnBuildingPicked handler).
//   - Place: click-click straight-edge footprint drawing (DraftPoints), finalizing into a new
//     FGrammarBuildingVolume appended to a dedicated hand-placed ABuildingInstancePoolActor.
//   - Move: drag an existing hand-placed building's footprint corner (classic transform-widget hooks,
//     same mechanism FlexNetwork/Landscape/BSP use for node dragging).
// Building footprints have no junctions/sharing between buildings (unlike FlexNetwork's road graph),
// so there is no shared node-graph subsystem here -- each building's footprint ring lives directly in
// its own ABuildingInstancePoolActor::SourceVolumes entry, edited in place and regenerated via
// RegenerateFromSource(), the same API the OSM-import path already uses.
class FBuildingPickEdMode : public FEdMode
{
public:
	static const FEditorModeID ModeID;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBuildingPicked, ABuildingInstancePoolActor* /*Pool*/, const FString& /*SourceName*/);
	static FOnBuildingPicked OnBuildingPicked;

	// User-declared (defined out-of-line in the .cpp, where FScopedTransaction is a complete type) so
	// TUniquePtr<FScopedTransaction>'s implicit destructor call doesn't need FScopedTransaction
	// complete at every translation unit that references this class -- same reason
	// FFlexNetworkEdMode declares its own destructor.
	virtual ~FBuildingPickEdMode() override;

	virtual void Enter() override;
	virtual void Exit() override;
	virtual bool UsesToolkits() const override { return true; }
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FBuildingPickEdMode"); }
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 X, int32 Y) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;

	// Move-tool node-selection gizmo (FLegacyEdModeWidgetHelper hooks -- the same mechanism
	// Landscape/BSP/FlexNetwork use for classic translate widgets).
	virtual bool ShouldDrawWidget() const override;
	virtual FVector GetWidgetLocation() const override;
	virtual bool AllowWidgetMove() override;
	virtual bool UsesTransformWidget() const override;
	virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const override;
	virtual bool StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override;

	UBuildingGrammarEdModeSettings* GetOrCreateModeSettings() const;

private:
	mutable TObjectPtr<UBuildingGrammarEdModeSettings> ModeSettings = nullptr;

	// ---------------------------------------------------------------- Select tool
	bool HandleSelectClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);

	TWeakObjectPtr<ABuildingInstancePoolActor> SelectedBuildingPool;
	FString SelectedBuildingSourceName;

	// ---------------------------------------------------------------- Place tool
	TArray<FVector> DraftPoints; // World cm, in the horizontal plane the user has been clicking on.
	FVector HoverWorldPoint = FVector::ZeroVector;
	bool bHoverValid = false;

	void BeginOrContinuePlacementClick();
	void CloseAndGenerateDraft();
	void CancelDraft();

	// Resolves (creating on first use) this level's single dedicated pool actor for hand-placed
	// buildings, tagged BuildingGrammarHandPlacedPoolTag so it's never mistaken for an OSM-import pool
	// (e.g. by "Delete All Generated Building Pools"/bReplaceExistingBuildingPools -- see this class's
	// .cpp header comment on FindOrCreateHandPlacedPool for that known limitation).
	ABuildingInstancePoolActor* FindOrCreateHandPlacedPool(bool bCreateIfMissing);

	// ---------------------------------------------------------------- Move tool
	TWeakObjectPtr<ABuildingInstancePoolActor> SelectedNodePool;
	FString SelectedNodeSourceName;
	int32 SelectedNodePointIndex = INDEX_NONE;
	FVector DraggedNodePosition = FVector::ZeroVector; // Live position shown during a drag; only written back to SourceVolumes on EndTracking.
	TUniquePtr<class FScopedTransaction> ActiveNodeDragTransaction;

	void ClearNodeSelection();
	void DeleteSelection();

	// Resolves a footprint ring vertex to a world position, substituting DraggedNodePosition for
	// whichever vertex is currently mid-drag so Render() shows the live position instead of the
	// stale one still in SourceVolumes (only written back on EndTracking).
	FVector GetFootprintPointWorld(ABuildingInstancePoolActor* Pool, const FString& SourceName, const TArray<FVector2D>& Ring, int32 Index) const;

	// ---------------------------------------------------------------- Shared helpers
	EBuildingGrammarEditTool GetActiveTool() const;

	// Casts a real ray into the world (landscape/static-mesh collision, ortho-camera-aware) to find
	// the point under the cursor; falls back to a flat plane (through the draft's start height while
	// placing, else world Z=0) so the tool still works in a completely empty level -- ported from
	// FFlexNetworkEdMode::TraceCursorToWorld.
	bool TraceCursorToWorld(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 X, int32 Y, FVector& OutPoint) const;
};
