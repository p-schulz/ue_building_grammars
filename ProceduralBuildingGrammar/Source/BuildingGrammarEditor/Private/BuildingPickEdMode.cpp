#include "BuildingPickEdMode.h"
#include "BuildingInstancePoolActor.h"
#include "Geometry/GrammarGeometry2D.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"

const FEditorModeID FBuildingPickEdMode::ModeID = TEXT("BuildingGrammar.PickBuildingMode");
FBuildingPickEdMode::FOnBuildingPicked FBuildingPickEdMode::OnBuildingPicked;

bool FBuildingPickEdMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
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
	// merged into this pool's shared HISM buckets/hero mesh) was actually clicked. Requires
	// ABuildingInstancePoolActor::AppendHeroMesh's EnableComplexAsSimpleCollision fix so hero-mesh
	// (wall/roof) surfaces are traceable at all -- HISM bucket instances already trace correctly.
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
			OnBuildingPicked.Broadcast(HitPool, Volume.SourceName);
			return true;
		}
	}

	return false;
}
