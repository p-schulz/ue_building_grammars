#include "BuildingGrammarEditorModule.h"
#include "ToolMenus.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "Editor.h"
#include "Misc/Paths.h"
#include "Osm/OsmTypes.h"
#include "BuildingGenerationLibrary.h"
#include "BuildingInstancePoolActor.h"
#include "Presets/GrammarBuildingPresets.h"

#define LOCTEXT_NAMESPACE "BuildingGrammarEditor"

// This is the plugin's one real, clickable editor entry point (see the Build.cs comment for why
// it's a plain Tools-menu command rather than an Editor Utility Widget Blueprint asset). It uses
// UToolMenus' standard "register a startup callback, add items inside it" pattern -- the same
// shape used throughout Epic's own editor modules -- so items are added once UToolMenus itself is
// ready rather than racing module load order.
void FBuildingGrammarEditorModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBuildingGrammarEditorModule::RegisterMenus));
}

void FBuildingGrammarEditorModule::ShutdownModule()
{
	UToolMenus::UnregisterOwner(this);
}

void FBuildingGrammarEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	if (!Menu)
	{
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection("ProceduralBuildingGrammar");
	Section.Label = LOCTEXT("SectionLabel", "Procedural Building Grammar");
	Section.AddMenuEntry(
		"GenerateFromOsm",
		LOCTEXT("GenerateFromOsm", "Generate Buildings from OSM..."),
		LOCTEXT("GenerateFromOsmTooltip", "Import an .osm file and generate procedural buildings into the current level"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnGenerateFromOsmClicked)));
}

// v1 flow, matching docs/PLAN.md section 7's scope: file-pick an .osm, derive a projection origin
// from the file's own node bounding-box center (no manual lat/lon entry dialog yet), generate with
// the urban_block Wave 1 preset (no preset picker yet either -- see GrammarBuildingPresets.h) into
// the currently open editor world. Kit meshes/materials still resolve to null (docs/PLAN.md
// section 4/6), so only facade walls/roof planes will be visible after this runs.
void FBuildingGrammarEditorModule::OnGenerateFromOsmClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	TArray<FString> OutFiles;
	const void* ParentWindowHandle = FSlateApplication::IsInitialized() ? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr) : nullptr;
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Select an OpenStreetMap .osm file"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("OpenStreetMap XML (*.osm)|*.osm"),
		EFileDialogFlags::None,
		OutFiles);

	if (!bOpened || OutFiles.Num() == 0)
	{
		return;
	}
	const FString OsmFilePath = OutFiles[0];

	FOsmDocument Document;
	FString ParseError;
	if (!FOsmDocument::ParseFile(OsmFilePath, Document, ParseError))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("ParseFailed", "Failed to parse '{0}': {1}"), FText::FromString(OsmFilePath), FText::FromString(ParseError)));
		return;
	}

	double CenterLatitude = 0.0;
	double CenterLongitude = 0.0;
	if (!Document.ComputeBoundsCenter(CenterLatitude, CenterLongitude))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoNodes", "The selected file has no nodes to generate buildings from."));
		return;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	// Document was already parsed once above just to find the projection origin;
	// GenerateBuildingsFromOsmFile parses the same file again internally. A small, acceptable
	// inefficiency for a menu-click tool rather than a hot path -- not worth restructuring the
	// library function to accept a pre-parsed document for this alone.
	const FBuildingGrammarConfig Config = GrammarBuildingPresets::UrbanBlockConfig();
	ABuildingInstancePoolActor* Pool = nullptr;
	const int32 GeneratedCount = UBuildingGenerationLibrary::GenerateBuildingsFromOsmFile(World, OsmFilePath, CenterLatitude, CenterLongitude, Config, Pool);

	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
		LOCTEXT("GeneratedCount", "Generated {0} building(s) from '{1}'."),
		FText::AsNumber(GeneratedCount),
		FText::FromString(FPaths::GetCleanFilename(OsmFilePath))));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBuildingGrammarEditorModule, BuildingGrammarEditor)
