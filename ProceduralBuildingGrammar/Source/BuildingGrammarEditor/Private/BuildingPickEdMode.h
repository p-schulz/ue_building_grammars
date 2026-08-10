#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

class ABuildingInstancePoolActor;

// Hidden editor mode (registered with bVisible=false -- see FBuildingGrammarEditorModule::StartupModule
// -- so it never appears in the Modes toolbar) toggled on/off only via the "Pick Building" Tools-menu
// entry. Its only real job is HandleClick: figure out which individual building (identified by
// FGrammarBuildingVolume::SourceName) was actually clicked, out of the many merged into a pool actor's
// shared HISM buckets/hero mesh -- see BuildingInstancePoolActor.h's own header comment for why
// individual buildings otherwise lose all identity once generated. Broadcasts the result on a static
// delegate rather than owning any UI itself, so FBuildingGrammarEditorModule owns showing/updating the
// customization details panel (see that module's OnBuildingPicked handler).
class FBuildingPickEdMode : public FEdMode
{
public:
	static const FEditorModeID ModeID;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBuildingPicked, ABuildingInstancePoolActor* /*Pool*/, const FString& /*SourceName*/);
	static FOnBuildingPicked OnBuildingPicked;

	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
};
