#include "BuildingGrammarEdModeToolkit.h"

#include "BuildingPickEdMode.h"
#include "BuildingGrammarEdModeSettings.h"
#include "EditorModeManager.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void FBuildingGrammarEdModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost)
{
	FModeToolkit::Init(InitToolkitHost);

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	const TSharedRef<IDetailsView> SettingsView = PropertyEditor.CreateDetailView(DetailsArgs);

	if (FBuildingPickEdMode* Mode = static_cast<FBuildingPickEdMode*>(GetEditorMode()))
	{
		SettingsView->SetObject(Mode->GetOrCreateModeSettings());
	}

	ToolkitWidget = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(NSLOCTEXT("BuildingGrammar", "EditorModeInstructions",
				"Select a FlexNetwork OSM asset and optional JSON grammar config below, then run generation. "
				"The OSM origin settings are shared with FlexNetwork, so buildings align with roads generated "
				"from the same asset. Click any generated building in the viewport to open its Building "
				"Customization window; committed changes regenerate that building pool."))
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SettingsView
		];
}

FText FBuildingGrammarEdModeToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("BuildingGrammar", "EditorModeToolkitName", "Building Grammar");
}

FEdMode* FBuildingGrammarEdModeToolkit::GetEditorMode() const
{
	if (!IsHosted())
	{
		return nullptr;
	}
	return GetEditorModeManager().GetActiveMode(FBuildingPickEdMode::ModeID);
}
