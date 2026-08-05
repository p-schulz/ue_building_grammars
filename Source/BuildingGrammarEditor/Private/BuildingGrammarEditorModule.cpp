#include "BuildingGrammarEditorModule.h"
#include "ToolMenus.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "Editor.h"
#include "Misc/Paths.h"
#include "Osm/OsmTypes.h"
#include "Config/GrammarConfigJson.h"
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
		"LoadConfigFromJson",
		LOCTEXT("LoadConfigFromJson", "Load Preset Config from JSON..."),
		LOCTEXT("LoadConfigFromJsonTooltip", "Load a facade/roof preset config from a JSON file using the Blender add-on's own schema (e.g. german_building_grammar_config.json). Used by 'Generate Buildings from OSM...' once loaded."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnLoadConfigFromJsonClicked)));
	Section.AddMenuEntry(
		"GenerateFromOsm",
		LOCTEXT("GenerateFromOsm", "Generate Buildings from OSM..."),
		LOCTEXT("GenerateFromOsmTooltip", "Import an .osm file and generate procedural buildings into the current level"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnGenerateFromOsmClicked)));
}

// Loads a config using the Blender add-on's own snake_case JSON schema (FGrammarConfigJson, not
// FBuildingGrammarConfig::FromJsonString's PascalCase round-trip format -- see
// GrammarConfigJson.h). The bundled german_building_grammar_config.json alone covers 25 of the 31
// facade styles from presets.py; combined with the 9 hand-ported in GrammarFacadeStyles.h, that's
// 30 of 31 without writing any more C++ style factories.
void FBuildingGrammarEditorModule::OnLoadConfigFromJsonClicked()
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
		TEXT("Select a preset config JSON file"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("JSON config (*.json)|*.json"),
		EFileDialogFlags::None,
		OutFiles);

	if (!bOpened || OutFiles.Num() == 0)
	{
		return;
	}

	FBuildingGrammarConfig Config;
	FString LoadError;
	if (!FGrammarConfigJson::LoadConfigFromPythonJsonFile(OutFiles[0], Config, LoadError))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("ConfigLoadFailed", "Failed to load '{0}': {1}"), FText::FromString(OutFiles[0]), FText::FromString(LoadError)));
		return;
	}

	LoadedConfig = Config;
	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
		LOCTEXT("ConfigLoaded", "Loaded {0} facade style(s) from '{1}'. 'Generate Buildings from OSM...' will use this config until a different one is loaded."),
		FText::AsNumber(Config.Styles.Num()),
		FText::FromString(FPaths::GetCleanFilename(OutFiles[0]))));
}

// v1 flow: file-pick an .osm, derive a projection origin from the file's own node bounding-box
// center (no manual lat/lon entry dialog yet), generate using whichever config
// OnLoadConfigFromJsonClicked most recently loaded (falling back to the built-in urban_block
// preset if none has been) into the currently open editor world. Kit meshes/materials are resolved
// via FGrammarKitResolver inside UBuildingGenerationLibrary::GenerateBuildingsFromOsmFile, which
// bakes the shared kit mesh + master material the first time it runs in this editor session.
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
	const FBuildingGrammarConfig Config = LoadedConfig.IsSet() ? LoadedConfig.GetValue() : GrammarBuildingPresets::UrbanBlockConfig();
	ABuildingInstancePoolActor* Pool = nullptr;
	const int32 GeneratedCount = UBuildingGenerationLibrary::GenerateBuildingsFromOsmFile(World, OsmFilePath, CenterLatitude, CenterLongitude, Config, Pool);

	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
		LOCTEXT("GeneratedCount", "Generated {0} building(s) from '{1}'."),
		FText::AsNumber(GeneratedCount),
		FText::FromString(FPaths::GetCleanFilename(OsmFilePath))));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBuildingGrammarEditorModule, BuildingGrammarEditor)
