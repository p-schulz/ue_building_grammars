#pragma once

#include "CoreMinimal.h"
#include "BuildingInstancePoolActor.h"
#include "Parcel/GrammarParcelTypes.h"
#include "BuildingPickPanelData.generated.h"

// Backing struct for the floating "Pick Building" customization details panel
// (FBuildingGrammarEditorModule::HandleBuildingPicked) -- wrapped in an FStructOnScope and shown via
// IStructureDetailsView, so edits to Override write straight back into this struct's live memory
// (see IStructureDetailsView.h's SetStructureData). SourceName/OriginalTags are read-only context for
// whoever is editing, not meant to be changed here.
USTRUCT()
struct FBuildingPickPanelData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Building")
	FString SourceName;

	UPROPERTY(VisibleAnywhere, Category = "Building")
	TMap<FString, FString> OriginalTags;

	UPROPERTY(EditAnywhere, Category = "Building")
	FBuildingCustomizationOverride Override;
};

// Backing struct for the floating "Pick Block" regenerate-parameters panel
// (FBuildingGrammarEditorModule::HandleBlockPicked) -- same FStructOnScope/IStructureDetailsView
// live-edit pattern as FBuildingPickPanelData above. BlockId is read-only context (identifies which
// block this panel is for); Method/ParcelConfig are what "Regenerate This Block" actually uses,
// pre-filled from the mode settings' current global values when the block was picked.
USTRUCT()
struct FGrammarBlockPickPanelData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Block")
	int32 BlockId = INDEX_NONE;

	UPROPERTY(EditAnywhere, Category = "Block")
	EGrammarParcelSubdivisionMethod Method = EGrammarParcelSubdivisionMethod::Hybrid;

	UPROPERTY(EditAnywhere, Category = "Block", meta = (ShowOnlyInnerProperties))
	FGrammarParcelConfig ParcelConfig;
};
