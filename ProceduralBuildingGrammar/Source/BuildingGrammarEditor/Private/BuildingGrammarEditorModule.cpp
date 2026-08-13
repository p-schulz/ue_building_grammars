#include "BuildingGrammarEditorModule.h"
#include "ToolMenus.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "Editor.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Osm/OsmTypes.h"
#include "Osm/BuildingFootprintAssembler.h"
#include "Config/GrammarConfigJson.h"
#include "BuildingGenerationLibrary.h"
#include "BuildingInstancePoolActor.h"
#include "BuildingActorPersistence.h"
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

#define LOCTEXT_NAMESPACE "BuildingGrammarEditor"

// This is the plugin's one real, clickable editor entry point (see the Build.cs comment for why
// it's a plain Tools-menu command rather than an Editor Utility Widget Blueprint asset). It uses
// UToolMenus' standard "register a startup callback, add items inside it" pattern -- the same
// shape used throughout Epic's own editor modules -- so items are added once UToolMenus itself is
// ready rather than racing module load order.
void FBuildingGrammarEditorModule::StartupModule()
{
	// bVisible=false keeps this out of the Modes toolbar entirely -- it's only ever activated via the
	// "Pick Building" Tools-menu entry below (OnPickBuildingClicked).
	FEditorModeRegistry::Get().RegisterMode<FBuildingPickEdMode>(FBuildingPickEdMode::ModeID, FText(), FSlateIcon(), /*bVisible=*/false);
	FBuildingPickEdMode::OnBuildingPicked.AddRaw(this, &FBuildingGrammarEditorModule::HandleBuildingPicked);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBuildingGrammarEditorModule::RegisterMenus));
}

void FBuildingGrammarEditorModule::ShutdownModule()
{
	FBuildingPickEdMode::OnBuildingPicked.RemoveAll(this);
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
		"GenerateFromOsm",
		LOCTEXT("GenerateFromOsm", "Generate Buildings from OSM..."),
		LOCTEXT("GenerateFromOsmTooltip", "Import an .osm file and generate procedural buildings into the current level"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnGenerateFromOsmClicked)));
	Section.AddMenuEntry(
		"GenerateFromOsmPCG",
		LOCTEXT("GenerateFromOsmPCG", "Generate Buildings from OSM (PCG)..."),
		LOCTEXT("GenerateFromOsmPCGTooltip", "Import an .osm file and generate buildings using the BuildingGrammarPCG module's alternative, PCG-graph-based pipeline instead of the deterministic engine. Runs asynchronously -- check the level and Output Log after a moment."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnGeneratePCGClicked)));
	Section.AddMenuEntry(
		"BakeToStaticMesh",
		LOCTEXT("BakeToStaticMesh", "Bake Generated Buildings to Static Meshes..."),
		LOCTEXT("BakeToStaticMeshTooltip", "Convert each generated building cell (selected pool actors, or every one in the level if none are selected) into one saved UStaticMesh asset, deleting the original pool actor and replacing it with a plain static mesh actor referencing the baked asset"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBuildingGrammarEditorModule::OnBakeToStaticMeshClicked)));
	Section.AddMenuEntry(
		"PickBuilding",
		LOCTEXT("PickBuilding", "Pick Building"),
		LOCTEXT("PickBuildingTooltip", "Toggle a viewport tool for clicking an individual generated building (out of the many merged into a pool actor's shared instances/hero mesh) to view/edit a per-building tag or facade-style override. Click this again, or click empty space, to stop picking."),
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

	// Origin is centered on the buildings themselves (FBuildingFootprintAssembler::
	// ComputeFootprintBoundsCenter), not on every node in the file -- a document's road/path/
	// boundary data commonly covers a larger and differently-shaped area than its building
	// footprints, so a bounds-center over every node can land far from where the buildings
	// actually are. GenerateBuildingsFromOsmFileChunked re-assembles footprints from the same file
	// internally (see its own comment on why re-parsing here is an accepted small inefficiency);
	// assembling them a second time here for this one call is the same acceptable cost.
	const TArray<FBuildingFootprint> Footprints = FBuildingFootprintAssembler::Assemble(Document);
	double CenterLatitude = 0.0;
	double CenterLongitude = 0.0;
	if (!FBuildingFootprintAssembler::ComputeFootprintBoundsCenter(Footprints, CenterLatitude, CenterLongitude))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoNodes", "The selected file has no building footprints to generate from."));
		return;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

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
	// just reduces per-cell memory/actor count by baking (and deleting the pool actor in favor of a
	// plain static mesh actor) immediately instead of leaving it as HISM buckets + a hero mesh
	// component. Can also be applied afterward via "Bake Generated Buildings to Static Meshes..."
	// independent of this choice.
	const bool bBakeToStaticMeshPerCell = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("AskBakeToStaticMesh",
		"Bake each cell to a static mesh asset as it's generated?\n\n"
		"Reduces memory/actor count during generation: each cell's pool actor is deleted and replaced with a plain static mesh actor referencing the baked asset. Baked cells lose per-instance HISM culling/streaming and can't be edited as individual instances afterward -- use \"Bake Generated Buildings to Static Meshes...\" later instead if you want to inspect/edit generated buildings first.")) == EAppReturnType::Yes;

	TArray<ABuildingInstancePoolActor*> Pools;
	FScopedSlowTask SlowTask(100.0f, LOCTEXT("GeneratingBuildings", "Generating buildings..."));
	SlowTask.MakeDialog(/*bShowCancelButton=*/true);

	bool bCancelled = false;
	const int32 GeneratedCount = UBuildingGenerationLibrary::GenerateBuildingsFromOsmFileChunked(
		World, OsmFilePath, CenterLatitude, CenterLongitude, Config, Pools,
		/*CellSize=*/10000.0, /*RuntimeGridName=*/NAME_None,
		bSaveAndUnloadPerCell, /*CellsPerLevelReload=*/25, bBakeToStaticMeshPerCell,
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

// Post-process counterpart to OnGenerateFromOsmClicked's bBakeToStaticMeshPerCell option -- bakes
// whatever ABuildingInstancePoolActors are already in the level (or just the current selection, if
// any) whenever the user is ready, decoupled from generation itself.
void FBuildingGrammarEditorModule::OnBakeToStaticMeshClicked()
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

	FScopedSlowTask SlowTask(static_cast<float>(Targets.Num()), LOCTEXT("BakingBuildings", "Baking buildings to static meshes..."));
	SlowTask.MakeDialog(/*bShowCancelButton=*/true);

	int32 BakedCount = 0;
	int32 SkippedCount = 0;
	for (ABuildingInstancePoolActor* Pool : Targets)
	{
		if (SlowTask.ShouldCancel())
		{
			break;
		}
		SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("BakingCell", "Baking '{0}'..."), FText::FromString(Pool->GetActorLabel())));

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
		LOCTEXT("BakeComplete", "Baked {0} cell(s) to static mesh assets under /Game/GeneratedBuildings/.{1}"),
		FText::AsNumber(BakedCount),
		SkippedCount > 0
			? FText::Format(LOCTEXT("BakeSkippedSuffix", " {0} had no geometry to bake and were left unchanged."), FText::AsNumber(SkippedCount))
			: FText::GetEmpty()));
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

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBuildingGrammarEditorModule, BuildingGrammarEditor)
