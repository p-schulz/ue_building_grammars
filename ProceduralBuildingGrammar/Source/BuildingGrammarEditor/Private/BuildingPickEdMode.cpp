#include "BuildingPickEdMode.h"
#include "BuildingGrammarEdModeSettings.h"
#include "BuildingGrammarEdModeToolkit.h"
#include "BuildingFootprintHitProxies.h"
#include "BuildingInstancePoolActor.h"
#include "Geometry/GrammarGeometry2D.h"
#include "EditorViewportClient.h"
#include "SceneView.h"
#include "SceneManagement.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "EngineDefines.h"
#include "ScopedTransaction.h"
#include "Misc/ScopedSlowTask.h"
#include "Toolkits/ToolkitManager.h"
#include "EditorModeManager.h"

const FEditorModeID FBuildingPickEdMode::ModeID = TEXT("BuildingGrammar.PickBuildingMode");
FBuildingPickEdMode::FOnBuildingPicked FBuildingPickEdMode::OnBuildingPicked;
FBuildingPickEdMode::FOnBlockPicked FBuildingPickEdMode::OnBlockPicked;

namespace
{
	// Marks the single dedicated pool actor this mode's Place tool appends hand-drawn buildings into,
	// so it's never mistaken for (and destroyed alongside) an OSM-import pool -- see
	// FBuildingPickEdMode::FindOrCreateHandPlacedPool.
	const FName HandPlacedPoolTag(TEXT("BuildingGrammarHandPlaced"));

	// World cm: clicking within this distance of the draft's first corner (Place tool) closes the
	// loop instead of adding another point.
	constexpr double CloseLoopRadiusCm = 100.0;
}

FBuildingPickEdMode::~FBuildingPickEdMode() = default;

void FBuildingPickEdMode::Enter()
{
	FEdMode::Enter();
	DraftPoints.Reset();
	ClearNodeSelection();

	if (!Toolkit.IsValid() && UsesToolkits())
	{
		Toolkit = MakeShareable(new FBuildingGrammarEdModeToolkit);
		Toolkit->Init(Owner->GetToolkitHost());
	}
}

void FBuildingPickEdMode::Exit()
{
	DraftPoints.Reset();
	ClearNodeSelection();
	ActiveNodeDragTransaction.Reset();
	SelectedBuildingPool = nullptr;
	SelectedBuildingSourceName.Reset();

	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}
	FEdMode::Exit();
}

void FBuildingPickEdMode::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdMode::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(ModeSettings);
}

UBuildingGrammarEdModeSettings* FBuildingPickEdMode::GetOrCreateModeSettings() const
{
	const UWorld* PreviousWorld = ModeSettings ? ModeSettings->TargetWorld.Get() : nullptr;
	if (!ModeSettings)
	{
		ModeSettings = NewObject<UBuildingGrammarEdModeSettings>(GetTransientPackage(), NAME_None, RF_Transactional);
	}
	ModeSettings->TargetWorld = GetWorld();
	if (ModeSettings->bUseFlexNetworkOsmContext && ModeSettings->TargetWorld.Get() != PreviousWorld)
	{
		ModeSettings->LoadFlexNetworkOsmContext();
	}
	return ModeSettings;
}

EBuildingGrammarEditTool FBuildingPickEdMode::GetActiveTool() const
{
	const UBuildingGrammarEdModeSettings* Settings = GetOrCreateModeSettings();
	return Settings ? Settings->ActiveTool : EBuildingGrammarEditTool::Select;
}

void FBuildingPickEdMode::RegenerateWithProgress(ABuildingInstancePoolActor* Pool, const FText& SlowTaskLabel)
{
	if (!Pool)
	{
		return;
	}

	// Below this many buildings the dialog would barely flash before the (already fast) regenerate
	// finishes -- FScopedSlowTask's own default un-shown grace period already avoids that, but
	// skipping MakeDialog() entirely for a trivially small pool also avoids the one-frame stall
	// creating the dialog window itself would otherwise add to what's already a near-instant edit.
	constexpr int32 MinVolumesToShowDialog = 4;
	FScopedSlowTask SlowTask(static_cast<float>(Pool->SourceVolumes.Num()), SlowTaskLabel);
	if (Pool->SourceVolumes.Num() >= MinVolumesToShowDialog)
	{
		SlowTask.MakeDialog(/*bShowCancelButton=*/true);
	}

	Pool->RegenerateFromSource([&SlowTask](int32 VolumesCompleted, int32 TotalVolumes, const FString& SourceName)
	{
		SlowTask.EnterProgressFrame(1.0f, FText::Format(
			NSLOCTEXT("BuildingGrammar", "RegeneratingBuilding", "Regenerating building {0}/{1} ({2})..."),
			FText::AsNumber(VolumesCompleted), FText::AsNumber(TotalVolumes), FText::FromString(SourceName)));
		return !SlowTask.ShouldCancel();
	});
}

// ---------------------------------------------------------------- Select tool

bool FBuildingPickEdMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	const EBuildingGrammarEditTool Tool = GetActiveTool();

	if (Tool == EBuildingGrammarEditTool::Place)
	{
		// Placement clicks are handled in InputKey instead, so drawing and node-selection clicks
		// (two different gestures) never fight over the same mouse-down event -- see
		// FFlexNetworkEdMode::HandleClick's identical reasoning.
		return false;
	}

	if (Tool == EBuildingGrammarEditTool::Move)
	{
		if (HitProxy && HitProxy->IsA(HBuildingFootprintNodeHitProxy::StaticGetType()))
		{
			HBuildingFootprintNodeHitProxy* NodeProxy = static_cast<HBuildingFootprintNodeHitProxy*>(HitProxy);
			if (ABuildingInstancePoolActor* Pool = NodeProxy->Pool.Get())
			{
				for (const FGrammarBuildingVolume& Volume : Pool->SourceVolumes)
				{
					if (Volume.SourceName == NodeProxy->SourceName && Volume.Footprint.OuterRing.IsValidIndex(NodeProxy->PointIndex))
					{
						SelectedNodePool = Pool;
						SelectedNodeSourceName = NodeProxy->SourceName;
						SelectedNodePointIndex = NodeProxy->PointIndex;
						const FVector2D& Point = Volume.Footprint.OuterRing[NodeProxy->PointIndex];
						DraggedNodePosition = FVector(Point.X * 100.0, Point.Y * 100.0, 0.0);
						if (InViewportClient)
						{
							InViewportClient->SetWidgetMode(UE::Widget::WM_Translate);
						}
						return true;
					}
				}
			}
		}
		ClearNodeSelection();
		return false;
	}

	if (Tool == EBuildingGrammarEditTool::Block)
	{
		return HandleBlockPickClick(InViewportClient, Click);
	}

	return HandleSelectClick(InViewportClient, HitProxy, Click);
}

bool FBuildingPickEdMode::HandleBlockPickClick(FEditorViewportClient* InViewportClient, const FViewportClick& Click)
{
	UBuildingGrammarEdModeSettings* Settings = GetOrCreateModeSettings();
	UWorld* World = GetWorld();
	if (!Settings || !World || Settings->LastParcelDebugData.IsEmpty())
	{
		return false;
	}

	const FVector Start = Click.GetOrigin();
	const FVector Direction = Click.GetDirection();
	const FVector End = Start + Direction * WORLD_MAX;

	FVector ImpactPoint;
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;
	if (World->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams))
	{
		ImpactPoint = HitResult.Location;
	}
	else
	{
		// Nothing under the cursor (e.g. a block with no landscape/road mesh under it yet) -- fall
		// back to the world Z=0 plane, same reasoning as TraceCursorToWorld's own fallback.
		if (FMath::Abs(Direction.Z) <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const double T = (0.0 - Start.Z) / Direction.Z;
		if (T < 0.0)
		{
			return false;
		}
		ImpactPoint = Start + Direction * T;
	}

	// Same footprint-space convention as HandleSelectClick: BlockBoundary is plain meters with no
	// separate origin bookkeeping, so this is a plain /100 on X/Y.
	const FVector2D PointMeters(ImpactPoint.X / 100.0, ImpactPoint.Y / 100.0);
	for (const FGrammarBlockDebugData& Debug : Settings->LastParcelDebugData)
	{
		if (!FGrammarGeometry2D::PointInRing(PointMeters, Debug.BlockBoundary))
		{
			continue;
		}

		FGrammarBlockPickInfo PickInfo;
		PickInfo.BlockId = Debug.BlockId;
		PickInfo.BlockBoundary = Debug.BlockBoundary;
		PickInfo.DominantRoadTagHint = Debug.DominantRoadTagHint;
		PickInfo.ParcelConfig = Settings->ParcelConfig;
		PickInfo.Method = Settings->ParcelSubdivisionMethod;
		OnBlockPicked.Broadcast(PickInfo);
		return true;
	}

	return false;
}

bool FBuildingPickEdMode::HandleSelectClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (!HitProxy || !HitProxy->IsA(HActor::StaticGetType()))
	{
		return FEdMode::HandleClick(InViewportClient, HitProxy, Click);
	}

	HActor* ActorHitProxy = static_cast<HActor*>(HitProxy);
	ABuildingInstancePoolActor* Pool = Cast<ABuildingInstancePoolActor>(ActorHitProxy->Actor.Get());
	if (!Pool)
	{
		return FEdMode::HandleClick(InViewportClient, HitProxy, Click);
	}

	UWorld* World = Pool->GetWorld();
	if (!World)
	{
		return false;
	}

	// Trace along the precise click ray rather than trusting the hit proxy/component bounds alone --
	// gives the exact world-space impact point needed to resolve which individual building (out of many
	// merged into this pool's shared HISM buckets/hero mesh) was actually clicked.
	const FVector TraceStart = Click.GetOrigin();
	const FVector TraceEnd = TraceStart + Click.GetDirection() * 1000000.0;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;

	FHitResult HitResult;
	if (!World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		return false;
	}

	ABuildingInstancePoolActor* HitPool = Cast<ABuildingInstancePoolActor>(HitResult.GetActor());
	if (!HitPool)
	{
		return false;
	}

	// FGrammarBuildingVolume::Footprint is stored in meters in the same local-tangent-plane space that
	// generation scales by exactly 100 (MetersToUnrealUnits -- see ApplyBuildingSpec) into absolute
	// UE-centimeter world-space, and pool actors always stay at identity transform -- so converting the
	// hit location back to footprint space is a plain /100 on X/Y, no origin bookkeeping needed.
	const FVector2D FootprintPoint(HitResult.Location.X / 100.0, HitResult.Location.Y / 100.0);
	for (const FGrammarBuildingVolume& Volume : HitPool->SourceVolumes)
	{
		if (FGrammarGeometry2D::PointInRing(FootprintPoint, Volume.Footprint.OuterRing))
		{
			SelectedBuildingPool = HitPool;
			SelectedBuildingSourceName = Volume.SourceName;
			OnBuildingPicked.Broadcast(HitPool, Volume.SourceName);
			return true;
		}
	}

	return false;
}

// ---------------------------------------------------------------- Place tool

ABuildingInstancePoolActor* FBuildingPickEdMode::FindOrCreateHandPlacedPool(bool bCreateIfMissing)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ABuildingInstancePoolActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(HandPlacedPoolTag))
		{
			return *It;
		}
	}

	if (!bCreateIfMissing)
	{
		return nullptr;
	}

	ABuildingInstancePoolActor* Pool = World->SpawnActor<ABuildingInstancePoolActor>();
	if (!Pool)
	{
		return nullptr;
	}
	Pool->Tags.Add(HandPlacedPoolTag);
	Pool->SetActorLabel(TEXT("BuildingGrammar_HandPlaced")); // BuildingGrammarEditor is an editor-only module, so SetActorLabel is always available here.
	return Pool;
}

void FBuildingPickEdMode::BeginOrContinuePlacementClick()
{
	if (!bHoverValid)
	{
		return;
	}

	// Snapped relative to the previous corner (no-op on the first corner of a draft, and a no-op
	// entirely unless the toggle is on) so the committed point always matches what Render() previewed.
	const FVector SnappedHover = DraftPoints.Num() > 0 ? ApplyAngleSnap(DraftPoints.Last(), HoverWorldPoint) : HoverWorldPoint;

	if (DraftPoints.Num() >= 3 && FVector::DistSquared2D(SnappedHover, DraftPoints[0]) <= FMath::Square(CloseLoopRadiusCm))
	{
		CloseAndGenerateDraft();
		return;
	}

	DraftPoints.Add(SnappedHover);
}

FVector FBuildingPickEdMode::ApplyAngleSnap(const FVector& From, const FVector& To) const
{
	const UBuildingGrammarEdModeSettings* Settings = GetOrCreateModeSettings();
	if (!Settings || !Settings->bAngleSnapEnabled)
	{
		return To;
	}

	const FVector Delta = To - From;
	const double HorizontalLength = Delta.Size2D();
	if (HorizontalLength <= KINDA_SMALL_NUMBER)
	{
		return To;
	}

	constexpr double AngleIncrementDegrees = 15.0;
	const double AngleIncrement = FMath::DegreesToRadians(AngleIncrementDegrees);
	const double Angle = FMath::Atan2(Delta.Y, Delta.X);
	const double SnappedAngle = static_cast<double>(FMath::RoundToInt32(Angle / AngleIncrement)) * AngleIncrement;

	return From + FVector(FMath::Cos(SnappedAngle) * HorizontalLength, FMath::Sin(SnappedAngle) * HorizontalLength, Delta.Z);
}

void FBuildingPickEdMode::CloseAndGenerateDraft()
{
	if (DraftPoints.Num() < 3)
	{
		CancelDraft();
		return;
	}

	ABuildingInstancePoolActor* Pool = FindOrCreateHandPlacedPool(/*bCreateIfMissing=*/true);
	if (!Pool)
	{
		CancelDraft();
		return;
	}

	UBuildingGrammarEdModeSettings* Settings = GetOrCreateModeSettings();

	FScopedTransaction Transaction(NSLOCTEXT("BuildingGrammar", "PlaceBuilding", "Place Building"));
	Pool->Modify();

	FGrammarBuildingVolume Volume;
	Volume.SourceName = FString::Printf(TEXT("hand/%s"), *FGuid::NewGuid().ToString());
	Volume.VolumeTags.Add(TEXT("building"), TEXT("yes"));
	Volume.Footprint.OuterRing.Reserve(DraftPoints.Num());
	for (const FVector& Point : DraftPoints)
	{
		Volume.Footprint.OuterRing.Add(FVector2D(Point.X / 100.0, Point.Y / 100.0));
	}

	// One config/style set is shared by every building in this pool (same as an OSM-import cell) --
	// refreshed from whatever's currently loaded every time a building is placed, so later edits to
	// GrammarConfigFile take effect on the next placement without needing to delete/recreate the pool.
	Pool->SourceConfig = Settings->GetResolvedConfigForPlacement();
	Pool->SourceVolumes.Add(Volume);

	if (!Settings->ActiveStyleName.IsEmpty() || Settings->bOverrideLevels || Settings->bOverrideRoofType)
	{
		FBuildingCustomizationOverride Override;
		Override.ForcedStyleName = Settings->ActiveStyleName;
		Override.bOverrideLevels = Settings->bOverrideLevels;
		Override.Levels = Settings->Levels;
		Override.bOverrideRoofType = Settings->bOverrideRoofType;
		Override.RoofType = Settings->RoofType;
		Pool->SetBuildingOverride(Volume.SourceName, Override);
	}

	// AppendVolume generates and appends just this one new building onto the pool's existing
	// components -- O(1) relative to how many buildings are already in the pool, unlike
	// RegenerateFromSource's full teardown+rebuild-everything (see that method's own comment). This
	// is what keeps placing a string of buildings one at a time from getting slower with each one.
	if (Pool->AppendVolume(Volume))
	{
		Settings->LastResult = FString::Printf(TEXT("Placed building '%s' (%d corners)."), *Volume.SourceName, Volume.Footprint.OuterRing.Num());
	}
	else
	{
		// Roll back so a failed placement (e.g. a self-intersecting/degenerate footprint) doesn't
		// leave an ungenerated ghost volume sitting in SourceVolumes.
		Pool->SourceVolumes.Pop();
		Pool->BuildingOverrides.Remove(Volume.SourceName);
		Settings->LastResult = TEXT("Failed to place building: the footprint could not be generated (likely self-intersecting or degenerate).");
	}

	DraftPoints.Reset();
}

void FBuildingPickEdMode::CancelDraft()
{
	DraftPoints.Reset();
}

// ---------------------------------------------------------------- Move tool

void FBuildingPickEdMode::ClearNodeSelection()
{
	SelectedNodePool = nullptr;
	SelectedNodeSourceName.Reset();
	SelectedNodePointIndex = INDEX_NONE;
}

bool FBuildingPickEdMode::ShouldDrawWidget() const
{
	return GetActiveTool() == EBuildingGrammarEditTool::Move && SelectedNodePointIndex != INDEX_NONE && SelectedNodePool.IsValid();
}

FVector FBuildingPickEdMode::GetWidgetLocation() const
{
	return ShouldDrawWidget() ? DraggedNodePosition : FVector::ZeroVector;
}

bool FBuildingPickEdMode::AllowWidgetMove()
{
	return ShouldDrawWidget();
}

bool FBuildingPickEdMode::UsesTransformWidget() const
{
	return ShouldDrawWidget();
}

bool FBuildingPickEdMode::UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const
{
	return ShouldDrawWidget() && CheckMode == UE::Widget::WM_Translate;
}

bool FBuildingPickEdMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (ShouldDrawWidget())
	{
		ActiveNodeDragTransaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("BuildingGrammar", "MoveFootprintNode", "Move Building Footprint Node"));
		return true;
	}
	return FEdMode::StartTracking(InViewportClient, InViewport);
}

bool FBuildingPickEdMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (ActiveNodeDragTransaction.IsValid())
	{
		ABuildingInstancePoolActor* Pool = SelectedNodePool.Get();
		if (Pool)
		{
			for (FGrammarBuildingVolume& Volume : Pool->SourceVolumes)
			{
				if (Volume.SourceName == SelectedNodeSourceName && Volume.Footprint.OuterRing.IsValidIndex(SelectedNodePointIndex))
				{
					Pool->Modify();
					Volume.Footprint.OuterRing[SelectedNodePointIndex] = FVector2D(DraggedNodePosition.X / 100.0, DraggedNodePosition.Y / 100.0);
					if (const UBuildingGrammarEdModeSettings* Settings = GetOrCreateModeSettings())
					{
						Pool->SourceConfig = Settings->GetResolvedConfigForPlacement();
					}
					RegenerateWithProgress(Pool, NSLOCTEXT("BuildingGrammar", "RegeneratingAfterMove", "Regenerating buildings after move..."));
					break;
				}
			}
		}
		// Closes the transaction now that the committed edit is inside it (must happen after the
		// mutation above, not before, or the edit lands outside the undo transaction).
		ActiveNodeDragTransaction.Reset();
		return true;
	}
	return FEdMode::EndTracking(InViewportClient, InViewport);
}

bool FBuildingPickEdMode::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (!ShouldDrawWidget() || InDrag.IsNearlyZero())
	{
		return FEdMode::InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
	}

	// Only the live preview position is updated per-tick (see Render); the actual SourceVolumes edit
	// and full-pool RegenerateFromSource() happen once, in EndTracking -- regenerating every building
	// in the pool on every InputDelta tick during a drag would be needlessly expensive.
	DraggedNodePosition += InDrag;
	return true;
}

// ---------------------------------------------------------------- Delete (both tools)

void FBuildingPickEdMode::DeleteSelection()
{
	const EBuildingGrammarEditTool Tool = GetActiveTool();

	if (Tool == EBuildingGrammarEditTool::Move && SelectedNodePointIndex != INDEX_NONE)
	{
		ABuildingInstancePoolActor* Pool = SelectedNodePool.Get();
		if (!Pool)
		{
			ClearNodeSelection();
			return;
		}
		const FString NodeSourceName = SelectedNodeSourceName;
		const int32 PointIndex = SelectedNodePointIndex;
		const int32 VolumeIndex = Pool->SourceVolumes.IndexOfByPredicate(
			[&NodeSourceName](const FGrammarBuildingVolume& Volume) { return Volume.SourceName == NodeSourceName; });
		ClearNodeSelection();
		if (VolumeIndex == INDEX_NONE)
		{
			return;
		}

		FScopedTransaction Transaction(NSLOCTEXT("BuildingGrammar", "DeleteFootprintNode", "Delete Building Footprint Node"));
		Pool->Modify();

		TArray<FVector2D>& Ring = Pool->SourceVolumes[VolumeIndex].Footprint.OuterRing;
		if (Ring.IsValidIndex(PointIndex))
		{
			Ring.RemoveAt(PointIndex);
		}
		if (Ring.Num() < 3)
		{
			// Can no longer form a polygon -- remove the whole building instead of leaving a degenerate one.
			Pool->BuildingOverrides.Remove(NodeSourceName);
			Pool->SourceVolumes.RemoveAt(VolumeIndex);
		}

		if (Pool->SourceVolumes.IsEmpty())
		{
			Pool->Destroy();
		}
		else
		{
			RegenerateWithProgress(Pool, NSLOCTEXT("BuildingGrammar", "RegeneratingAfterNodeDelete", "Regenerating buildings after deleting node..."));
		}
		return;
	}

	if (Tool == EBuildingGrammarEditTool::Select && SelectedBuildingPool.IsValid())
	{
		ABuildingInstancePoolActor* Pool = SelectedBuildingPool.Get();
		const FString RemovedSourceName = SelectedBuildingSourceName;
		SelectedBuildingPool = nullptr;
		SelectedBuildingSourceName.Reset();

		FScopedTransaction Transaction(NSLOCTEXT("BuildingGrammar", "DeleteBuilding", "Delete Building"));
		Pool->Modify();
		Pool->SourceVolumes.RemoveAll(
			[&RemovedSourceName](const FGrammarBuildingVolume& Volume) { return Volume.SourceName == RemovedSourceName; });
		Pool->BuildingOverrides.Remove(RemovedSourceName);

		if (Pool->SourceVolumes.IsEmpty())
		{
			Pool->Destroy();
		}
		else
		{
			RegenerateWithProgress(Pool, NSLOCTEXT("BuildingGrammar", "RegeneratingAfterBuildingDelete", "Regenerating buildings after delete..."));
		}
	}
}

// ---------------------------------------------------------------- Input / render

bool FBuildingPickEdMode::TraceCursorToWorld(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 X, int32 Y, FVector& OutPoint) const
{
	if (!ViewportClient || !Viewport)
	{
		return false;
	}

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags).SetRealtimeUpdate(ViewportClient->IsRealtime()));
	FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
	if (!View)
	{
		return false;
	}

	const FViewportCursorLocation CursorLocation(View, ViewportClient, X, Y);
	const FVector Direction = CursorLocation.GetDirection();
	FVector Start = CursorLocation.GetOrigin();

	// In orthographic views the "camera" sits arbitrarily far back along the view direction from
	// whatever's under the cursor -- pull the ray origin back first, exactly like Landscape's own
	// cursor trace, or top-down/ortho clicks miss entirely.
	if (ViewportClient->IsOrtho())
	{
		Start -= WORLD_MAX * Direction;
	}
	const FVector End = Start + WORLD_MAX * Direction;

	if (UWorld* World = GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BuildingGrammarCursorTrace), /*bTraceComplex=*/ true);
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			OutPoint = Hit.Location;
			return true;
		}
	}

	// Nothing under the cursor (e.g. a level with no landscape/floor yet) -- fall back to a flat
	// plane so the tool still works, through the draft's own start height while placing, else world Z=0.
	const float PlaneZ = DraftPoints.Num() > 0 ? static_cast<float>(DraftPoints[0].Z) : 0.f;
	if (FMath::Abs(Direction.Z) <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const float T = (PlaneZ - Start.Z) / Direction.Z;
	if (T < 0.f)
	{
		return false;
	}
	OutPoint = Start + Direction * T;
	return true;
}

bool FBuildingPickEdMode::MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 X, int32 Y)
{
	FVector WorldPoint;
	bHoverValid = TraceCursorToWorld(ViewportClient, Viewport, X, Y, WorldPoint);
	if (bHoverValid)
	{
		HoverWorldPoint = WorldPoint;
	}
	return true;
}

bool FBuildingPickEdMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	const EBuildingGrammarEditTool Tool = GetActiveTool();

	if (Tool == EBuildingGrammarEditTool::Place)
	{
		if (Key == EKeys::LeftMouseButton)
		{
			if (Event == IE_Pressed)
			{
				BeginOrContinuePlacementClick();
			}
			return true; // Own the whole click stream in Place tool -- see FFlexNetworkEdMode's identical reasoning.
		}
		if (Key == EKeys::Enter && Event == IE_Pressed && DraftPoints.Num() >= 3)
		{
			CloseAndGenerateDraft();
			return true;
		}
		if ((Key == EKeys::RightMouseButton || Key == EKeys::Escape) && Event == IE_Pressed && DraftPoints.Num() > 0)
		{
			CancelDraft();
			return true;
		}
	}

	if ((Key == EKeys::Delete || Key == EKeys::Platform_Delete) && Event == IE_Pressed)
	{
		if ((Tool == EBuildingGrammarEditTool::Move && SelectedNodePointIndex != INDEX_NONE)
			|| (Tool == EBuildingGrammarEditTool::Select && SelectedBuildingPool.IsValid()))
		{
			DeleteSelection();
			return true;
		}
	}

	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

FVector FBuildingPickEdMode::GetFootprintPointWorld(ABuildingInstancePoolActor* Pool, const FString& SourceName, const TArray<FVector2D>& Ring, int32 Index) const
{
	if (SelectedNodePool.Get() == Pool && SelectedNodeSourceName == SourceName && SelectedNodePointIndex == Index)
	{
		return DraggedNodePosition;
	}
	return FVector(Ring[Index].X * 100.0, Ring[Index].Y * 100.0, 0.0);
}

// Draws the most recent "Generate Buildings From Road Network" run's captured debug data (see
// UBuildingGrammarEdModeSettings::LastParcelDebugData/bShowParcelDebugVisualization): each block's
// boundary, the subdivision algorithm's own debug rays/boxes (OBB split cuts, skeleton frontage rays,
// inner offset/collapsed contours), and the final parcel outlines color-coded by outcome. Independent
// of which viewport tool (Select/Place/Move) is active -- this is a passive overlay, not a tool.
static void DrawParcelDebugVisualization(const UBuildingGrammarEdModeSettings* Settings, FPrimitiveDrawInterface* PDI)
{
	if (!Settings || !Settings->bShowParcelDebugVisualization)
	{
		return;
	}

	const auto ToWorld = [](const FVector2D& PointMeters, double WorldZ)
	{
		return FVector(PointMeters.X * 100.0, PointMeters.Y * 100.0, WorldZ);
	};
	const auto DrawRing = [&](const TArray<FVector2D>& Ring, double WorldZ, const FColor& Color, float Thickness)
	{
		const int32 N = Ring.Num();
		for (int32 i = 0; i < N; ++i)
		{
			PDI->DrawLine(ToWorld(Ring[i], WorldZ), ToWorld(Ring[(i + 1) % N], WorldZ), Color, SDPG_Foreground, Thickness);
		}
	};

	for (const FGrammarBlockDebugData& Debug : Settings->LastParcelDebugData)
	{
		DrawRing(Debug.BlockBoundary, Debug.WorldZ, FColor(120, 130, 255, 180), 1.5f);

		// Block-diagonal half-length to clamp OBB split lines to -- GrammarParcelSubdivision.cpp
		// stores those as a 200km-long segment centered on the actual split point (Pivot), since it
		// has no idea how big the block is; drawn as-is that would be a line stretching far off into
		// the distance instead of a useful visual.
		FVector2D MinP(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
		FVector2D MaxP(-TNumericLimits<double>::Max(), -TNumericLimits<double>::Max());
		for (const FVector2D& P : Debug.BlockBoundary)
		{
			MinP = FVector2D(FMath::Min(MinP.X, P.X), FMath::Min(MinP.Y, P.Y));
			MaxP = FVector2D(FMath::Max(MaxP.X, P.X), FMath::Max(MaxP.Y, P.Y));
		}
		const double BlockHalfDiagonal = FVector2D::Distance(MinP, MaxP) * 0.55;

		for (const FGrammarParcelDebugPolygon& DebugPoly : Debug.Subdivision.Polygons)
		{
			const FColor Color = DebugPoly.Kind == TEXT("obb") ? FColor(255, 255, 255, 55) : FColor(255, 0, 255, 90);
			DrawRing(DebugPoly.Polygon, Debug.WorldZ, Color, 1.f);
		}

		for (const FGrammarParcelDebugLine& DebugLine : Debug.Subdivision.Lines)
		{
			FVector2D A = DebugLine.A;
			FVector2D B = DebugLine.B;
			FColor Color(0, 200, 255, 140);
			if (DebugLine.Kind == TEXT("skeleton-ray"))
			{
				Color = FColor(255, 230, 0, 140);
			}
			else if (DebugLine.Kind == TEXT("skeleton-ray-rejected"))
			{
				Color = FColor(255, 0, 0, 90);
			}
			else if (DebugLine.Kind == TEXT("obb-split"))
			{
				const FVector2D Mid = (A + B) * 0.5;
				const FVector2D Dir = (B - A).GetSafeNormal();
				A = Mid - Dir * BlockHalfDiagonal;
				B = Mid + Dir * BlockHalfDiagonal;
			}
			PDI->DrawLine(ToWorld(A, Debug.WorldZ), ToWorld(B, Debug.WorldZ), Color, SDPG_Foreground, 1.f);
		}

		for (const FGrammarParcel& Parcel : Debug.Subdivision.Parcels)
		{
			FColor Color(80, 220, 80, 200); // Buildable, no warning.
			if (Parcel.Method == TEXT("patio"))
			{
				Color = FColor(160, 80, 220, 150);
			}
			else if (Parcel.Warning == TEXT("unsplit") || !Parcel.bStreetAccess)
			{
				Color = FColor(220, 60, 60, 170); // Rejected -- not generated.
			}
			else if (!Parcel.Warning.IsEmpty())
			{
				Color = FColor(255, 180, 60, 200); // Generated anyway, but flagged (area/width out of range).
			}
			DrawRing(Parcel.Polygon, Debug.WorldZ, Color, 2.5f);
		}
	}
}

void FBuildingPickEdMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);
	if (!PDI)
	{
		return;
	}

	DrawParcelDebugVisualization(GetOrCreateModeSettings(), PDI);

	const EBuildingGrammarEditTool Tool = GetActiveTool();

	if (Tool == EBuildingGrammarEditTool::Move)
	{
		if (ABuildingInstancePoolActor* Pool = FindOrCreateHandPlacedPool(/*bCreateIfMissing=*/false))
		{
			for (const FGrammarBuildingVolume& Volume : Pool->SourceVolumes)
			{
				const TArray<FVector2D>& Ring = Volume.Footprint.OuterRing;
				const int32 NumPoints = Ring.Num();
				for (int32 Index = 0; Index < NumPoints; ++Index)
				{
					const bool bSelected = SelectedNodePool == Pool && SelectedNodeSourceName == Volume.SourceName && SelectedNodePointIndex == Index;
					const FVector WorldPos = GetFootprintPointWorld(Pool, Volume.SourceName, Ring, Index);
					const FVector NextWorldPos = GetFootprintPointWorld(Pool, Volume.SourceName, Ring, (Index + 1) % NumPoints);

					PDI->DrawLine(WorldPos, NextWorldPos, FColor(255, 255, 255, 128), SDPG_Foreground, 2.f);

					PDI->SetHitProxy(new HBuildingFootprintNodeHitProxy(Pool, Volume.SourceName, Index));
					PDI->DrawPoint(WorldPos, bSelected ? FColor::Yellow : FColor::White, bSelected ? 16.f : 10.f, SDPG_Foreground);
					PDI->SetHitProxy(nullptr);
				}
			}
		}
	}

	if (Tool == EBuildingGrammarEditTool::Place && DraftPoints.Num() > 0)
	{
		for (int32 Index = 0; Index < DraftPoints.Num(); ++Index)
		{
			PDI->DrawPoint(DraftPoints[Index], Index == 0 ? FColor::Green : FColor::White, 12.f, SDPG_Foreground);
			if (Index > 0)
			{
				PDI->DrawLine(DraftPoints[Index - 1], DraftPoints[Index], FColor::Green, SDPG_Foreground, 3.f);
			}
		}
		if (bHoverValid)
		{
			// Snapped the same way BeginOrContinuePlacementClick snaps the actual commit, so the
			// preview line never shows a different edge than the one a click would actually place.
			const FVector SnappedHover = ApplyAngleSnap(DraftPoints.Last(), HoverWorldPoint);
			const bool bWouldClose = DraftPoints.Num() >= 3 && FVector::DistSquared2D(SnappedHover, DraftPoints[0]) <= FMath::Square(CloseLoopRadiusCm);
			PDI->DrawLine(DraftPoints.Last(), bWouldClose ? DraftPoints[0] : SnappedHover, bWouldClose ? FColor::Yellow : FColor(0, 255, 0, 128), SDPG_Foreground, 2.f);
		}
	}
}
