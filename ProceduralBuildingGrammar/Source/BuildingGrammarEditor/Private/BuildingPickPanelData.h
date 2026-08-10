#pragma once

#include "CoreMinimal.h"
#include "BuildingInstancePoolActor.h"
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
