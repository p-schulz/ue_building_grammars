#include "BuildingGrammarEditorModule.h"
#include "ToolMenus.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "Editor.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Osm/BuildingGrammarOsmTypes.h"
#include "Config/GrammarConfigJson.h"
#include "BuildingGenerationLibrary.h"
#include "BuildingInstancePoolActor.h"
#include "BuildingActorPersistence.h"
#include "FlexRoadBlockExtraction.h"
#include "GeoReferenceOriginActor.h"
#include "Presets/GrammarBuildingPresets.h"
#include "EngineUtils.h"
#include "Selection.h"
#include "BuildingPickEdMode.h"
#include "BuildingPickPanelData.h"
#include "EditorModeRegistry.h"
#include "EditorModeManager.h"
#include "PropertyEditorModule.h"
#include "IStructureDetailsView.h"
#include "UObject/StructOnScope.h"
#include "Widgets/SWindow.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "Helpers/PCGGraphParametersHelpers.h"
#include "TreeImportLibrary.h"
#include "TreeInstancePoolActor.h"
#include "ScopedTransaction.h"
#include "BuildingGrammarEdModeSettings.h"

#define LOCTEXT_NAMESPACE "BuildingGrammarEditor"

// Registers both the visible editor mode and the legacy Tools-menu commands. The mode adds the
// persistent OSM-asset workflow; the menu entries remain useful shortcuts for the original
// file-based import/bake operations.
void FBuildingGrammarEditorModule::StartupModule()
{
	FEditorModeRegistry::Get().RegisterMode<FBuildingPickEdMode>(
		FBuildingPickEdMode::ModeID,
		LOCTEXT("BuildingGrammarModeName", "Building Grammar"),
		FSlateIcon(),
		/*bVisible=*/true);
	FBuildingPickEdMode::OnBuildingPicked.AddRaw(this, &FBuildingGrammarEditorModule::HandleBuildingPicked);
	FBuildingPickEdMode::OnBlockPicked.AddRaw(this, &FBuildingGrammarEditorModule::HandleBlockPicked);
	FEditorDelegates::OnMapLoad.AddRaw(this, &FBuildingGrammarEditorModule::HandleMapLoad);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBuildingGrammarEditorModule::RegisterMenus));
}

void FBuildingGrammarEditorModule::ShutdownModule()
{
	FBuildingPickEdMode::OnBuildingPicked.RemoveAll(this);
	FBuildingPickEdMode::OnBlockPicked.RemoveAll(this);
	FEditorDelegates::OnMapLoad.RemoveAll(this);
	// UnrealEd may already be gone by the time a plugin module shuts down (no strict ordering
	// guarantee) -- guard rather than risk calling into a torn-down singleton.
	if (FModuleManager::Get().IsModuleLoaded(TEXT("UnrealEd")))
	{
		FEditorModeRegistry::Get().UnregisterMode(FBuildingPickEdMode::ModeID);
	}

	if (PickPanelWindow.IsValid())
	{
		PickPanelWindow->RequestDestroyWindow();
		PickPanelWindow.Reset();
	}
	PickPanelDetailsView.Reset();
	PickPanelStruct.Reset();

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
		"SetLevelGeoReference",
		LOCTEXT("SetLevelGeoReference", "Set Level Geo Reference..."),
		LOCTEXT("SetLevelGeoReferenceTooltip", "Pin the shared projection origin every OSM import in this level snaps to, from a chosen file's own bounds. Lets multiple OSM extracts imported one after another stitch together instead of each landing centered on its own bounds."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnSetLevelGeoReferenceClicked)));
	Section.AddMenuEntry(
		"ClearLevelGeoReference",
		LOCTEXT("ClearLevelGeoReference", "Clear Level Geo Reference"),
		LOCTEXT("ClearLevelGeoReferenceTooltip", "Remove this level's shared projection origin, so the next OSM import re-establishes one from its own file's bounds instead of snapping to whatever was set before."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnClearLevelGeoReferenceClicked)));
	Section.AddMenuEntry(
		"GenerateFromOsm",
		LOCTEXT("GenerateFromOsm", "Generate Buildings from OSM..."),
		LOCTEXT("GenerateFromOsmTooltip", "Import an .osm file and generate procedural buildings into the current level"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnGenerateFromOsmClicked)));
	Section.AddMenuEntry(
		"GenerateFromRoadNetwork",
		LOCTEXT("GenerateFromRoadNetwork", "Generate Buildings from Road Network..."),
		LOCTEXT("GenerateFromRoadNetworkTooltip", "Extract city blocks from the current level's FlexNetwork road graph, subdivide each into parcels, and generate a building on every street-facing parcel."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnGenerateFromRoadNetworkClicked)));
	Section.AddMenuEntry(
		"GenerateFromOsmPCG",
		LOCTEXT("GenerateFromOsmPCG", "Generate Buildings from OSM (PCG)..."),
		LOCTEXT("GenerateFromOsmPCGTooltip", "Import an .osm file and generate buildings using the BuildingGrammarPCG module's alternative, PCG-graph-based pipeline instead of the deterministic engine. Runs asynchronously -- check the level and Output Log after a moment."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnGeneratePCGClicked)));
	Section.AddMenuEntry(
		"SaveToStaticMeshes",
		LOCTEXT("SaveToStaticMeshes", "Save to Static Meshes"),
		LOCTEXT("SaveToStaticMeshesTooltip", "Convert each generated building cell (selected pool actors, or every one in the level if none are selected) into one saved UStaticMesh asset, deleting the original pool actor and replacing it with a plain static mesh actor referencing the baked asset"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnSaveToStaticMeshesClicked)));
	Section.AddMenuEntry(
		"BakeToLevelLightweight",
		LOCTEXT("BakeToLevelLightweight", "Bake to Level (Lightweight)"),
		LOCTEXT("BakeToLevelLightweightTooltip", "Clear each generated building cell's derived HISM/hero-mesh geometry (selected pool actors, or every one in the level if none are selected) in place, keeping its authored source data so it automatically regenerates the next time this level loads. Unlike 'Save to Static Meshes', creates no new assets and keeps per-building edit/regenerate capability -- it only shrinks what's serialized in the meantime."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnBakeToLevelLightweightClicked)));
	Section.AddMenuEntry(
		"ImportTreesFromGeoJson",
		LOCTEXT("ImportTreesFromGeoJson", "Import Trees from GeoJSON..."),
		LOCTEXT("ImportTreesFromGeoJsonTooltip", "Import a GeoJSON tree-cadastre export, filtered to a loaded OpenStreetMap file's own region, as one instanced tree pool actor. Assign meshes per tree type in Project Settings > Plugins > Procedural Building Grammar - Trees first, or nothing will render."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnImportTreesFromGeoJsonClicked)));
	Section.AddMenuEntry(
		"DeleteAllBuildingPools",
		LOCTEXT("DeleteAllBuildingPools", "Delete All Generated Building Pools"),
		LOCTEXT("DeleteAllBuildingPoolsTooltip", "Delete every generated building pool actor in the current level. Closing/reopening a level or the editor with many live pool actors can otherwise leave the editor window unresponsive during teardown -- run this first to avoid that."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnDeleteAllBuildingPoolsClicked)));
	Section.AddMenuEntry(
		"PickBuilding",
		LOCTEXT("PickBuilding", "Building Grammar Mode"),
		LOCTEXT("PickBuildingTooltip", "Toggle the Building Grammar editor mode for OSM-asset generation and clicking individual generated buildings to edit their configuration."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnPickBuildingClicked),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([]() { return GLevelEditorModeTools().IsModeActive(FBuildingPickEdMode::ModeID); })),
		EUserInterfaceActionType::ToggleButton);
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

void FBuildingGrammarEditorModule::OnSetLevelGeoReferenceClicked()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	double ExistingLatitude = 0.0, ExistingLongitude = 0.0;
	if (AGeoReferenceOriginActor::FindInWorld(World, ExistingLatitude, ExistingLongitude))
	{
		const EAppReturnType::Type Confirm = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(
			LOCTEXT("ConfirmChangeGeoReference", "This level already has a geo reference ({0}, {1}). Changing it will misalign anything already generated against the old one. Continue?"),
			FText::AsNumber(ExistingLatitude), FText::AsNumber(ExistingLongitude)));
		if (Confirm != EAppReturnType::Yes)
		{
			return;
		}
	}

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	TArray<FString> OutFiles;
	const void* ParentWindowHandle = FSlateApplication::IsInitialized() ? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr) : nullptr;
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Select an OpenStreetMap .osm file to derive the geo reference from"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("OpenStreetMap XML (*.osm)|*.osm"),
		EFileDialogFlags::None,
		OutFiles);
	if (!bOpened || OutFiles.Num() == 0)
	{
		return;
	}

	FOsmDocument Document;
	FString ParseError;
	if (!FOsmDocument::ParseFile(OutFiles[0], Document, ParseError))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("GeoReferenceParseFailed", "Failed to parse '{0}': {1}"), FText::FromString(OutFiles[0]), FText::FromString(ParseError)));
		return;
	}

	double Latitude = 0.0, Longitude = 0.0;
	if (!Document.GetBoundsCenter(Latitude, Longitude))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("GeoReferenceNoBounds", "The selected file has no <bounds> element and no nodes to derive a geo reference from."));
		return;
	}

	AGeoReferenceOriginActor::SetInWorld(World, Latitude, Longitude);
	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
		LOCTEXT("GeoReferenceSet", "Level geo reference set to ({0}, {1}) from '{2}'. Every OSM import in this level will now snap to it."),
		FText::AsNumber(Latitude), FText::AsNumber(Longitude), FText::FromString(FPaths::GetCleanFilename(OutFiles[0]))));
}

void FBuildingGrammarEditorModule::OnClearLevelGeoReferenceClicked()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	TArray<AGeoReferenceOriginActor*> Existing;
	for (TActorIterator<AGeoReferenceOriginActor> It(World); It; ++It)
	{
		Existing.Add(*It);
	}
	if (Existing.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoGeoReferenceToClear", "This level has no geo reference set."));
		return;
	}

	const EAppReturnType::Type Confirm = FMessageDialog::Open(EAppMsgType::YesNo,
		LOCTEXT("ConfirmClearGeoReference", "Clear this level's geo reference? The next OSM import will establish a new one from its own file's bounds."));
	if (Confirm != EAppReturnType::Yes)
	{
		return;
	}

	for (AGeoReferenceOriginActor* Actor : Existing)
	{
		Actor->Destroy();
	}
}

// v1 flow: file-pick an .osm, derive a projection origin from the file's own node bounding-box
// center (no manual lat/lon entry dialog yet), generate using whichever config
// OnLoadConfigFromJsonClicked most recently loaded (falling back to the built-in urban_block
// preset if none has been) into the currently open editor world. Kit meshes/materials are resolved
// via FGrammarKitResolver inside UBuildingGenerationLibrary::GenerateBuildingsFromOsmFileChunked,
// which bakes the shared kit mesh + master material the first time it runs in this editor session,
// and automatically splits the extract into one ABuildingInstancePoolActor per grid cell (see
// FBuildingVolumeGrid) rather than a single unbounded pool -- keeps each pool World-Partition-
// friendly once saved, and lets this progress dialog show/cancel per cell instead of freezing the
// editor with no feedback for the whole (potentially multi-minute) run.
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

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	// Fallback origin is the midpoint of the file's own <bounds> element (or, lacking one, of every
	// node's own Lat/Lon extent -- see FOsmDocument::GetBounds/GetBoundsCenter). If this level already
	// has a geo reference (AGeoReferenceOriginActor -- set explicitly via "Set Level Geo Reference..."
	// or established automatically by an earlier import), that existing reference wins instead, so
	// this file lands stitched against whatever was already generated rather than recentered on its
	// own bounds. If not, this file's own bounds-center becomes the new reference, covering the
	// first-import-in-a-level case automatically.
	double FallbackLatitude = 0.0;
	double FallbackLongitude = 0.0;
	if (!Document.GetBoundsCenter(FallbackLatitude, FallbackLongitude))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoNodes", "The selected file has no <bounds> element and no nodes to generate from."));
		return;
	}
	double CenterLatitude = 0.0;
	double CenterLongitude = 0.0;
	AGeoReferenceOriginActor::ResolveOrigin(World, FallbackLatitude, FallbackLongitude, CenterLatitude, CenterLongitude);

	// Document was already parsed once above just to find the projection origin;
	// GenerateBuildingsFromOsmFileChunked parses the same file again internally. A small, acceptable
	// inefficiency for a menu-click tool rather than a hot path -- not worth restructuring the
	// library function to accept a pre-parsed document for this alone.
	const FBuildingGrammarConfig Config = LoadedConfig.IsSet() ? LoadedConfig.GetValue() : GrammarBuildingPresets::UrbanBlockConfig();

	// Only offered when the current level actually has World Partition -- see
	// FBuildingActorPersistence's comment for why that's a hard precondition, not a soft one. Every
	// other level in this project keeps today's behavior (pools stay live/visible immediately).
	bool bSaveAndUnloadPerCell = false;
	if (FBuildingActorPersistence::IsWorldPartitioned(World))
	{
		bSaveAndUnloadPerCell = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("AskSaveAndUnload",
			"This level has World Partition enabled. Save each generated cell to disk and periodically free memory by saving and reloading the level (recommended for large areas -- avoids the out-of-memory failure a very large extract can otherwise hit)?\n\n"
			"The editor will briefly reload the level every so often during generation. Generated buildings won't be visible in this level until World Partition streams that cell back in.")) == EAppReturnType::Yes;
	}

	// Not WP-gated -- unlike bSaveAndUnloadPerCell, this has nothing to do with World Partition; it
	// just reduces per-cell memory by clearing each cell's HISM buckets + hero mesh component
	// immediately after it's generated, keeping its source data so it regenerates automatically the
	// next time the pool loads (see ABuildingInstancePoolActor::BakeToLevelLightweight). Unlike the
	// permanent "Save to Static Meshes" conversion, the pool actor itself isn't deleted and stays
	// fully editable/regeneratable afterward -- this only shrinks its footprint during the run.
	const bool bBakeToLevelPerCell = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("AskBakeToLevel",
		"Bake each cell to the level (lightweight) as it's generated?\n\n"
		"Reduces memory during generation: each cell's HISM buckets and hero mesh geometry are cleared right after it's generated, keeping its source data so it regenerates automatically the next time this level loads. Unlike \"Save to Static Meshes\", no new assets are created and the pool actor stays fully editable/regeneratable -- use \"Save to Static Meshes\" later instead if you want a permanent, merged static mesh.")) == EAppReturnType::Yes;

	TArray<ABuildingInstancePoolActor*> Pools;
	FScopedSlowTask SlowTask(100.0f, LOCTEXT("GeneratingBuildings", "Generating buildings..."));
	SlowTask.MakeDialog(/*bShowCancelButton=*/true);

	bool bCancelled = false;
	const int32 GeneratedCount = UBuildingGenerationLibrary::GenerateBuildingsFromOsmFileChunked(
		World, OsmFilePath, CenterLatitude, CenterLongitude, Config, Pools,
		/*CellSize=*/10000.0, /*RuntimeGridName=*/NAME_None,
		bSaveAndUnloadPerCell, /*CellsPerLevelReload=*/25, bBakeToLevelPerCell,
		[&SlowTask, &bCancelled](int32 CellsCompleted, int32 TotalCells) -> bool
		{
			const float WorkThisCell = TotalCells > 0 ? 100.0f / static_cast<float>(TotalCells) : 100.0f;
			SlowTask.EnterProgressFrame(WorkThisCell, FText::Format(
				LOCTEXT("GeneratingCell", "Generating cell {0}/{1}..."), FText::AsNumber(CellsCompleted), FText::AsNumber(TotalCells)));
			bCancelled = SlowTask.ShouldCancel();
			return !bCancelled;
		});

	// Pools stays empty when bSaveAndUnloadPerCell is set (see GenerateBuildingsFromOsmFileChunked's
	// comment) -- there's nothing safe to report a pointer to, so the completion message reports
	// buildings generated only rather than a misleading "0 pool(s)".
	const FText Message = bSaveAndUnloadPerCell
		? FText::Format(
			bCancelled ? LOCTEXT("GeneratedCountCancelledSaved", "Cancelled after generating {0} building(s) from '{1}' (each cell saved to disk; the level was periodically reloaded to free memory).")
			           : LOCTEXT("GeneratedCountSaved", "Generated {0} building(s) from '{1}' (each cell saved to disk; the level was periodically reloaded to free memory)."),
			FText::AsNumber(GeneratedCount),
			FText::FromString(FPaths::GetCleanFilename(OsmFilePath)))
		: FText::Format(
			bCancelled ? LOCTEXT("GenerationCancelled", "Cancelled after generating {0} building(s) across {1} pool(s) from '{2}'.")
			           : LOCTEXT("GeneratedCount", "Generated {0} building(s) across {1} pool(s) from '{2}'."),
			FText::AsNumber(GeneratedCount),
			FText::AsNumber(Pools.Num()),
			FText::FromString(FPaths::GetCleanFilename(OsmFilePath)));

	FMessageDialog::Open(EAppMsgType::Ok, Message);
}

// Extracts every block from the level's current FlexNetwork road graph, subdivides each into
// parcels, and generates a building on every street-facing parcel. Unlike OnGenerateFromOsmClicked
// there's no file to pick -- the road network is whatever's already live in the level -- so this
// goes straight to extraction, then runs the (potentially slow) subdivision + generation pass behind
// a cancellable FScopedSlowTask. Uses default parcel-subdivision settings (Hybrid method, default
// FGrammarParcelConfig) -- the ed-mode toolkit's "Generate Buildings From Road Network" command
// (UBuildingGrammarEdModeSettings::GenerateBuildingsFromRoadNetwork) is the one that exposes
// ParcelConfig/ParcelSubdivisionMethod for tuning; this menu entry is a quick, same-defaults shortcut.
void FBuildingGrammarEditorModule::OnGenerateFromRoadNetworkClicked()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	const TArray<FGrammarBlockInput> Blocks = FFlexRoadBlockExtraction::ExtractBlockInputsFromFlexNetwork(World);
	if (Blocks.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoRoadNetworkBlocks", "No closed blocks were found in this level's FlexNetwork road graph. Draw or import a road network first (Tools > FlexNetwork)."));
		return;
	}

	const FBuildingGrammarConfig Config = LoadedConfig.IsSet() ? LoadedConfig.GetValue() : GrammarBuildingPresets::UrbanBlockConfig();

	TArray<ABuildingInstancePoolActor*> Pools;
	FScopedSlowTask SlowTask(static_cast<float>(Blocks.Num()), LOCTEXT("GeneratingBuildingsFromRoadNetwork", "Generating buildings from road network..."));
	SlowTask.MakeDialog(/*bShowCancelButton=*/true);

	bool bCancelled = false;
	const int32 GeneratedCount = UBuildingGenerationLibrary::GenerateBuildingsFromBlocks(
		World, Blocks, FGrammarParcelConfig(), EGrammarParcelSubdivisionMethod::Hybrid, Config, Pools,
		/*CellSize=*/10000.0, /*RuntimeGridName=*/NAME_None,
		[&SlowTask, &bCancelled](int32 BlocksCompleted, int32 TotalBlocks) -> bool
		{
			SlowTask.EnterProgressFrame(1.0f, FText::Format(
				LOCTEXT("GeneratingBlock", "Subdividing block {0}/{1}..."), FText::AsNumber(BlocksCompleted), FText::AsNumber(TotalBlocks)));
			bCancelled = SlowTask.ShouldCancel();
			return !bCancelled;
		});

	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
		bCancelled ? LOCTEXT("GeneratedFromRoadNetworkCancelled", "Cancelled after generating {0} building(s) across {1} block(s) into {2} pool(s).")
		           : LOCTEXT("GeneratedFromRoadNetwork", "Generated {0} building(s) across {1} block(s) into {2} pool(s)."),
		FText::AsNumber(GeneratedCount), FText::AsNumber(Blocks.Num()), FText::AsNumber(Pools.Num())));
}

// Permanent counterpart to OnGenerateFromOsmClicked's bBakeToLevelPerCell option -- bakes whatever
// ABuildingInstancePoolActors are already in the level (or just the current selection, if any) to
// merged UStaticMesh assets whenever the user is ready, decoupled from generation itself.
void FBuildingGrammarEditorModule::OnSaveToStaticMeshesClicked()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	TArray<ABuildingInstancePoolActor*> Targets;
	if (USelection* Selection = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator It(*Selection); It; ++It)
		{
			if (ABuildingInstancePoolActor* Pool = Cast<ABuildingInstancePoolActor>(*It))
			{
				Targets.Add(Pool);
			}
		}
	}
	if (Targets.IsEmpty())
	{
		for (TActorIterator<ABuildingInstancePoolActor> It(World); It; ++It)
		{
			Targets.Add(*It);
		}
	}

	if (Targets.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoPoolsToBake", "No generated building pool actors found in this level (or in the current selection)."));
		return;
	}

	FScopedSlowTask SlowTask(static_cast<float>(Targets.Num()), LOCTEXT("SavingBuildings", "Saving buildings to static meshes..."));
	SlowTask.MakeDialog(/*bShowCancelButton=*/true);

	int32 BakedCount = 0;
	int32 SkippedCount = 0;
	for (ABuildingInstancePoolActor* Pool : Targets)
	{
		if (SlowTask.ShouldCancel())
		{
			break;
		}
		SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("SavingCell", "Saving '{0}'..."), FText::FromString(Pool->GetActorLabel())));

		if (ABuildingInstancePoolActor::BakeAndReplace(Pool, Pool->MakeDefaultBakedAssetPath()))
		{
			++BakedCount;
		}
		else
		{
			++SkippedCount;
		}
	}

	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
		LOCTEXT("SaveComplete", "Saved {0} cell(s) to static mesh assets under /Game/GeneratedBuildings/.{1}"),
		FText::AsNumber(BakedCount),
		SkippedCount > 0
			? FText::Format(LOCTEXT("SaveSkippedSuffix", " {0} had no geometry to save and were left unchanged."), FText::AsNumber(SkippedCount))
			: FText::GetEmpty()));
}

// Lightweight counterpart to OnSaveToStaticMeshesClicked -- see ABuildingInstancePoolActor::
// BakeToLevelLightweight's own comment for what "lightweight" means here (no new assets, derived
// geometry regenerates automatically from source data on next load instead of being replaced).
void FBuildingGrammarEditorModule::OnBakeToLevelLightweightClicked()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	TArray<ABuildingInstancePoolActor*> Targets;
	if (USelection* Selection = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator It(*Selection); It; ++It)
		{
			if (ABuildingInstancePoolActor* Pool = Cast<ABuildingInstancePoolActor>(*It))
			{
				Targets.Add(Pool);
			}
		}
	}
	if (Targets.IsEmpty())
	{
		for (TActorIterator<ABuildingInstancePoolActor> It(World); It; ++It)
		{
			Targets.Add(*It);
		}
	}

	if (Targets.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoPoolsToLightweightBake", "No generated building pool actors found in this level (or in the current selection)."));
		return;
	}

	FScopedSlowTask SlowTask(static_cast<float>(Targets.Num()), LOCTEXT("LightweightBakingBuildings", "Baking buildings to level (lightweight)..."));
	SlowTask.MakeDialog(/*bShowCancelButton=*/true);

	int32 BakedCount = 0;
	int32 SkippedCount = 0;
	for (ABuildingInstancePoolActor* Pool : Targets)
	{
		if (SlowTask.ShouldCancel())
		{
			break;
		}
		SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("LightweightBakingCell", "Baking '{0}'..."), FText::FromString(Pool->GetActorLabel())));

		if (Pool->SourceVolumes.Num() == 0)
		{
			++SkippedCount;
			continue;
		}
		Pool->BakeToLevelLightweight();
		++BakedCount;
	}

	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
		LOCTEXT("LightweightBakeComplete", "Baked {0} cell(s) to the level (lightweight).{1}"),
		FText::AsNumber(BakedCount),
		SkippedCount > 0
			? FText::Format(LOCTEXT("LightweightBakeSkippedSuffix", " {0} had no source data to regenerate from and were left unchanged."), FText::AsNumber(SkippedCount))
			: FText::GetEmpty()));
}

// Shared by OnDeleteAllBuildingPoolsClicked (manual, with its own confirmation/progress UI) and
// HandleMapLoad (automatic, silent) -- see either caller's own comment for why deleting these
// matters. Plain Destroy() per actor, same as UBuildingStreamingSubsystem::DeactivateCell's own
// cleanup -- no special-cased component teardown needed.
int32 FBuildingGrammarEditorModule::DeleteAllBuildingPools(UWorld* World)
{
	if (!World)
	{
		return 0;
	}

	TArray<ABuildingInstancePoolActor*> Targets;
	for (TActorIterator<ABuildingInstancePoolActor> It(World); It; ++It)
	{
		Targets.Add(*It);
	}

	int32 DeletedCount = 0;
	for (ABuildingInstancePoolActor* Pool : Targets)
	{
		if (Pool->Destroy())
		{
			++DeletedCount;
		}
	}
	return DeletedCount;
}

// Teardown workaround: a level (or the editor itself) with many live ABuildingInstancePoolActors
// -- each owning a UDynamicMeshComponent hero mesh plus one UHierarchicalInstancedStaticMeshComponent
// per (Role, VariantKey) bucket -- can leave the editor window unresponsive while closing/reopening
// the level, since teardown has to destroy every one of those components/instances at once. Deleting
// the pool actors first (here, while the editor is still responsive) avoids hitting that during the
// level transition itself. See HandleMapLoad for the automatic counterpart to this manual action.
void FBuildingGrammarEditorModule::OnDeleteAllBuildingPoolsClicked()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	int32 PoolCount = 0;
	for (TActorIterator<ABuildingInstancePoolActor> It(World); It; ++It)
	{
		++PoolCount;
	}

	if (PoolCount == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoPoolsToDelete", "No generated building pool actors found in this level."));
		return;
	}

	const EAppReturnType::Type Confirm = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(
		LOCTEXT("ConfirmDeleteAllPools", "Delete all {0} generated building pool actor(s) in this level? This cannot be undone."),
		FText::AsNumber(PoolCount)));
	if (Confirm != EAppReturnType::Yes)
	{
		return;
	}

	FScopedSlowTask SlowTask(1.0f, LOCTEXT("DeletingBuildingPools", "Deleting building pools..."));
	SlowTask.MakeDialog();
	const int32 DeletedCount = DeleteAllBuildingPools(World);

	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("DeleteAllPoolsComplete", "Deleted {0} building pool actor(s)."), FText::AsNumber(DeletedCount)));
}

// Automatic counterpart to OnDeleteAllBuildingPoolsClicked -- see this function's own declaration
// comment in the header for why FEditorDelegates::OnMapLoad is the right hook (fires before the
// current level's teardown begins, covers both "Open Level" and "New Level") and why editor-close
// isn't handled the same way. Deliberately silent (log only): this can fire on every level
// open/new-level, so a confirmation dialog or summary popup every time would be intrusive for what
// is, by design, a routine cleanup step rather than something the user needs to approve each time.
// OutCanLoadMap is left untouched (defaults to true, i.e. never vetoes the load) -- there's no
// reason for a cleanup step to block loading the new level.
void FBuildingGrammarEditorModule::HandleMapLoad(const FString& Filename, FCanLoadMap& OutCanLoadMap)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	const int32 DeletedCount = DeleteAllBuildingPools(World);
	if (DeletedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("BuildingGrammarEditor: deleted %d building pool actor(s) before loading '%s' (avoids the editor becoming unresponsive while the old level tears down)."), DeletedCount, *Filename);
	}
}

// Alternative to OnGenerateFromOsmClicked: drives the BuildingGrammarPCG module's PCG-graph-based
// pipeline instead of the deterministic C++ engine (see that module's own header comment for how the
// two relate). Reuses an existing UPCGComponent already pointed at the graph -- preferring the
// current selection, else searching the whole level -- so repeated clicks drive the same actor
// instead of spawning a new one each time; only spawns a fresh actor if none is found. Sets the
// picked file on the component's "OsmFilePath" Graph Parameter (must already be exposed on the graph
// -- see this plugin's PCG wiring notes) and triggers Generate. PCG graph execution is scheduled
// asynchronously by UPCGSubsystem, so this can only report that generation was started, not a
// completed building/pool count the way OnGenerateFromOsmClicked's dialog can.
void FBuildingGrammarEditorModule::OnGeneratePCGClicked()
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

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	const TCHAR* GraphAssetPath = TEXT("/ProceduralBuildingGrammar/BP_BuildingGrammarPCG.BP_BuildingGrammarPCG");
	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, GraphAssetPath);
	if (!Graph)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("PCGGraphNotFound", "Could not load the PCG graph asset at '{0}'. Has it been moved or renamed?"),
			FText::FromString(GraphAssetPath)));
		return;
	}

	UPCGComponent* TargetComponent = nullptr;
	if (USelection* Selection = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator It(*Selection); It; ++It)
		{
			if (AActor* SelectedActor = Cast<AActor>(*It))
			{
				if (UPCGComponent* Component = SelectedActor->FindComponentByClass<UPCGComponent>())
				{
					if (Component->GetGraph() == Graph)
					{
						TargetComponent = Component;
						break;
					}
				}
			}
		}
	}
	if (!TargetComponent)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (UPCGComponent* Component = It->FindComponentByClass<UPCGComponent>())
			{
				if (Component->GetGraph() == Graph)
				{
					TargetComponent = Component;
					break;
				}
			}
		}
	}

	if (!TargetComponent)
	{
		AActor* SpawnedActor = World->SpawnActor<AActor>();
		if (!SpawnedActor)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PCGSpawnFailed", "Failed to spawn an actor for the PCG graph."));
			return;
		}
		SpawnedActor->SetActorLabel(TEXT("BuildingGrammarPCG_Generated"));

		TargetComponent = NewObject<UPCGComponent>(SpawnedActor, TEXT("PCGComponent"));
		TargetComponent->SetGraph(Graph);
		SpawnedActor->AddInstanceComponent(TargetComponent);
		TargetComponent->RegisterComponent();
	}

	UPCGGraphParametersHelpers::SetStringParameter(TargetComponent->GetGraphInstance(), TEXT("OsmFilePath"), OsmFilePath);
	TargetComponent->Generate(/*bForce=*/true);

	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
		LOCTEXT("PCGGenerationStarted", "Started PCG generation on '{0}' from '{1}'. PCG graph execution runs asynchronously -- check the level shortly for results, and the Output Log for any node errors (e.g. \"No OSM file path set\" means the graph doesn't expose an 'OsmFilePath' Graph Parameter yet, or it isn't wired to the data-source nodes)."),
		FText::FromString(TargetComponent->GetOwner()->GetActorLabel()),
		FText::FromString(FPaths::GetCleanFilename(OsmFilePath))));
}

// Two file-pick dialogs (GeoJSON, then the .osm defining the origin/region), then a single
// UTreeImportLibrary::ImportTreesFromGeoJson call -- see that function's own header comment for the
// filtering/projection/ground-snap/random-rotation details, and this class's own header comment on
// UTreeMeshSettings for why nothing may render until meshes are assigned there.
void FBuildingGrammarEditorModule::OnImportTreesFromGeoJsonClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	TArray<FString> GeoJsonFiles;
	const void* ParentWindowHandle = FSlateApplication::IsInitialized() ? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr) : nullptr;
	const bool bPickedGeoJson = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Select a GeoJSON tree file"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("GeoJSON (*.geojson;*.json)|*.geojson;*.json"),
		EFileDialogFlags::None,
		GeoJsonFiles);
	if (!bPickedGeoJson || GeoJsonFiles.Num() == 0)
	{
		return;
	}

	TArray<FString> OsmFiles;
	const bool bPickedOsm = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Select the OpenStreetMap .osm file defining the region to filter trees into"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("OpenStreetMap XML (*.osm)|*.osm"),
		EFileDialogFlags::None,
		OsmFiles);
	if (!bPickedOsm || OsmFiles.Num() == 0)
	{
		return;
	}
	const FString OsmFilePath = OsmFiles[0];

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	// Same projection-origin derivation OnGenerateFromOsmClicked uses for this same file (including
	// AGeoReferenceOriginActor::ResolveOrigin -- an existing level geo reference wins over this file's
	// own bounds), so trees line up with whatever buildings were (or will be) generated in this level.
	FOsmDocument Document;
	FString ParseError;
	if (!FOsmDocument::ParseFile(OsmFilePath, Document, ParseError))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("TreeOsmParseFailed", "Failed to parse '{0}': {1}"), FText::FromString(OsmFilePath), FText::FromString(ParseError)));
		return;
	}
	double FallbackLatitude = 0.0;
	double FallbackLongitude = 0.0;
	if (!Document.GetBoundsCenter(FallbackLatitude, FallbackLongitude))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("TreeOsmNoFootprints", "The selected OSM file has no <bounds> element and no nodes to derive a projection origin from."));
		return;
	}
	double CenterLatitude = 0.0;
	double CenterLongitude = 0.0;
	AGeoReferenceOriginActor::ResolveOrigin(World, FallbackLatitude, FallbackLongitude, CenterLatitude, CenterLongitude);

	FString ImportError;
	ATreeInstancePoolActor* Pool = UTreeImportLibrary::ImportTreesFromGeoJson(
		World, GeoJsonFiles[0], OsmFilePath, CenterLatitude, CenterLongitude, /*bSnapToGround=*/true, /*RandomSeed=*/12345, ImportError);

	if (!Pool)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("TreeImportFailed", "Failed to import trees: {0}"), FText::FromString(ImportError)));
		return;
	}

	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
		LOCTEXT("TreeImportComplete", "Imported {0} tree instance(s) across {1} mesh bucket(s) from '{2}', filtered to '{3}'s region.\n\nIf that's 0, assign meshes per tree type in Project Settings > Plugins > Procedural Building Grammar - Trees -- trees whose type has no mesh configured are skipped rather than spawned with a placeholder."),
		FText::AsNumber(Pool->NumInstances()),
		FText::AsNumber(Pool->NumBuckets()),
		FText::FromString(FPaths::GetCleanFilename(GeoJsonFiles[0])),
		FText::FromString(FPaths::GetCleanFilename(OsmFilePath))));
}

void FBuildingGrammarEditorModule::OnPickBuildingClicked()
{
	GLevelEditorModeTools().ActivateMode(FBuildingPickEdMode::ModeID, /*bToggle=*/true);
}

// Shows (creating on first use) a floating details panel over an FBuildingPickPanelData for the
// just-picked building, pre-filled with its existing override (if any) -- see
// BuildingPickPanelData.h. Reused across picks rather than recreated each time: avoids
// FGlobalTabmanager registration complexity for what's a single, occasional-use tool. If the user
// previously closed the window via its native close button, PickPanelWindow/PickPanelDetailsView
// were reset to null (see the SetOnWindowClosed binding below) and are transparently recreated here.
void FBuildingGrammarEditorModule::HandleBuildingPicked(ABuildingInstancePoolActor* Pool, const FString& SourceName)
{
	if (!Pool)
	{
		return;
	}

	PickedPool = Pool;

	FBuildingPickPanelData PanelData;
	PanelData.SourceName = SourceName;
	for (const FGrammarBuildingVolume& Volume : Pool->SourceVolumes)
	{
		if (Volume.SourceName == SourceName)
		{
			PanelData.OriginalTags = Volume.VolumeTags;
			break;
		}
	}
	if (const FBuildingCustomizationOverride* Existing = Pool->BuildingOverrides.Find(SourceName))
	{
		PanelData.Override = *Existing;
	}

	PickPanelStruct = MakeShared<FStructOnScope>(FBuildingPickPanelData::StaticStruct());
	*reinterpret_cast<FBuildingPickPanelData*>(PickPanelStruct->GetStructMemory()) = PanelData;

	if (!PickPanelDetailsView.IsValid())
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bShowOptions = false;
		const FStructureDetailsViewArgs StructureViewArgs;
		PickPanelDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, PickPanelStruct, LOCTEXT("PickPanelTitle", "Building Customization"));
		// No separate "Apply" button -- edits take effect on the picked building as soon as a field
		// commits (see HandlePickPanelPropertyChanged).
		PickPanelDetailsView->GetOnFinishedChangingPropertiesDelegate().AddRaw(this, &FBuildingGrammarEditorModule::HandlePickPanelPropertyChanged);
	}
	else
	{
		PickPanelDetailsView->SetStructureData(PickPanelStruct);
	}

	if (!PickPanelWindow.IsValid())
	{
		PickPanelWindow = SNew(SWindow)
			.Title(LOCTEXT("PickPanelWindowTitle", "Building Customization"))
			.ClientSize(FVector2D(380.0f, 260.0f))
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			[
				PickPanelDetailsView->GetWidget().ToSharedRef()
			];
		PickPanelWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda([this](const TSharedRef<SWindow>&)
		{
			// The details view's widget lived inside the now-destroyed native window -- drop both so
			// the next pick recreates them from scratch instead of reusing a widget with nowhere to go.
			PickPanelDetailsView.Reset();
			PickPanelWindow.Reset();
		}));
		FSlateApplication::Get().AddWindow(PickPanelWindow.ToSharedRef());
	}
	else
	{
		PickPanelWindow->ShowWindow();
		PickPanelWindow->BringToFront();
	}
}

void FBuildingGrammarEditorModule::HandlePickPanelPropertyChanged(const FPropertyChangedEvent& ChangedEvent)
{
	if (!PickPanelStruct.IsValid() || !PickedPool.IsValid())
	{
		return;
	}

	const FBuildingPickPanelData* PanelData = reinterpret_cast<const FBuildingPickPanelData*>(PickPanelStruct->GetStructMemory());
	if (!PanelData)
	{
		return;
	}

	ABuildingInstancePoolActor* Pool = PickedPool.Get();
	Pool->SetBuildingOverride(PanelData->SourceName, PanelData->Override);
	Pool->RegenerateFromSource();
}

// Shows (creating on first use) a floating "Block Regenerate" panel for the just-picked block,
// pre-filled with the mode's current global ParcelConfig/Method (see FGrammarBlockPickInfo) --
// same reused-window pattern as HandleBuildingPicked, but entirely separate FStructOnScope/window so
// both panels can be open at once without fighting over one.
void FBuildingGrammarEditorModule::HandleBlockPicked(const FGrammarBlockPickInfo& Info)
{
	PickedBlockBoundary = Info.BlockBoundary;
	PickedBlockTagHint = Info.DominantRoadTagHint;

	FGrammarBlockPickPanelData PanelData;
	PanelData.BlockId = Info.BlockId;
	PanelData.Method = Info.Method;
	PanelData.ParcelConfig = Info.ParcelConfig;

	BlockPickPanelStruct = MakeShared<FStructOnScope>(FGrammarBlockPickPanelData::StaticStruct());
	*reinterpret_cast<FGrammarBlockPickPanelData*>(BlockPickPanelStruct->GetStructMemory()) = PanelData;

	if (!BlockPickPanelDetailsView.IsValid())
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bShowOptions = false;
		const FStructureDetailsViewArgs StructureViewArgs;
		BlockPickPanelDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, BlockPickPanelStruct, LOCTEXT("BlockPickPanelTitle", "Block Regenerate"));
		// No separate "Regenerate" button -- edits take effect on the picked block as soon as a field
		// commits (see HandleBlockPickPanelPropertyChanged), same convention as the building panel.
		BlockPickPanelDetailsView->GetOnFinishedChangingPropertiesDelegate().AddRaw(this, &FBuildingGrammarEditorModule::HandleBlockPickPanelPropertyChanged);
	}
	else
	{
		BlockPickPanelDetailsView->SetStructureData(BlockPickPanelStruct);
	}

	if (!BlockPickPanelWindow.IsValid())
	{
		BlockPickPanelWindow = SNew(SWindow)
			.Title(LOCTEXT("BlockPickPanelWindowTitle", "Block Regenerate"))
			.ClientSize(FVector2D(420.0f, 460.0f))
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			[
				BlockPickPanelDetailsView->GetWidget().ToSharedRef()
			];
		BlockPickPanelWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda([this](const TSharedRef<SWindow>&)
		{
			BlockPickPanelDetailsView.Reset();
			BlockPickPanelWindow.Reset();
		}));
		FSlateApplication::Get().AddWindow(BlockPickPanelWindow.ToSharedRef());
	}
	else
	{
		BlockPickPanelWindow->ShowWindow();
		BlockPickPanelWindow->BringToFront();
	}
}

// Regenerates exactly one block in place: removes its existing volumes (by SourceName prefix
// "parcel/{BlockId}_") from whichever pool(s) currently hold them, destroying any pool left with
// none, then generates fresh parcels for it via GenerateBuildingsFromBlocks. The remove-then-generate
// split is required because GenerateBuildingsFromResolvedVolumes (which GenerateBuildingsFromBlocks
// funnels into) always spawns a brand-new ABuildingInstancePoolActor per call -- it never reuses an
// existing one -- so calling it again for the same block without first clearing the old volumes would
// leave duplicate, overlapping buildings instead of replacing them.
void FBuildingGrammarEditorModule::HandleBlockPickPanelPropertyChanged(const FPropertyChangedEvent& ChangedEvent)
{
	if (!BlockPickPanelStruct.IsValid())
	{
		return;
	}
	const FGrammarBlockPickPanelData* PanelData = reinterpret_cast<const FGrammarBlockPickPanelData*>(BlockPickPanelStruct->GetStructMemory());
	if (!PanelData)
	{
		return;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	// Reuses whichever grammar config/cell-size/runtime-grid the ed-mode's own "Generate Buildings
	// From Road Network" command is currently configured with, so a single-block regenerate produces
	// output consistent with the batch run it's touching up -- rather than this module's own,
	// independently-tracked LoadedConfig, which may point at a different file entirely.
	FBuildingGrammarConfig Config;
	double CellSize = 10000.0;
	FName RuntimeGridName = NAME_None;
	if (FBuildingPickEdMode* Mode = static_cast<FBuildingPickEdMode*>(GLevelEditorModeTools().GetActiveMode(FBuildingPickEdMode::ModeID)))
	{
		if (UBuildingGrammarEdModeSettings* Settings = Mode->GetOrCreateModeSettings())
		{
			Config = Settings->GetResolvedConfigForPlacement();
			CellSize = Settings->CellSize;
			RuntimeGridName = Settings->RuntimeGridName;
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("RegenerateBlock", "Regenerate Block"));

	const FString SourceNamePrefix = FString::Printf(TEXT("parcel/%d_"), PanelData->BlockId);
	TArray<ABuildingInstancePoolActor*> ExistingPools;
	for (TActorIterator<ABuildingInstancePoolActor> It(World); It; ++It)
	{
		ExistingPools.Add(*It);
	}
	for (ABuildingInstancePoolActor* Pool : ExistingPools)
	{
		const int32 RemovedCount = Pool->SourceVolumes.RemoveAll([&SourceNamePrefix](const FGrammarBuildingVolume& Volume)
		{
			return Volume.SourceName.StartsWith(SourceNamePrefix);
		});
		if (RemovedCount == 0)
		{
			continue;
		}
		Pool->Modify();
		if (Pool->SourceVolumes.IsEmpty())
		{
			Pool->Destroy();
		}
		else
		{
			Pool->RegenerateFromSource();
		}
	}

	FGrammarBlockInput BlockInput;
	BlockInput.Boundary = PickedBlockBoundary;
	BlockInput.DominantRoadTagHint = PickedBlockTagHint;
	BlockInput.BlockId = PanelData->BlockId;

	TArray<ABuildingInstancePoolActor*> NewPools;
	UBuildingGenerationLibrary::GenerateBuildingsFromBlocks(
		World, {BlockInput}, PanelData->ParcelConfig, PanelData->Method, Config, NewPools, CellSize, RuntimeGridName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBuildingGrammarEditorModule, BuildingGrammarEditor)
