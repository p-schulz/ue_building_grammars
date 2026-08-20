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
				"OSM workflow: select a FlexNetwork OSM asset and optional JSON grammar config below, then "
				"run generation. The OSM origin settings are shared with FlexNetwork, so buildings align "
				"with roads generated from the same asset.\n\n"
				"Hand-placement: set Active Tool below.\n"
				"• Place: click to add footprint corners connected by straight edges; click near the "
				"first corner (or press Enter, 3+ corners) to close the loop and generate a building with "
				"the selected Active Style Name (or Auto for tag-based selection); Escape/right-click cancels.\n"
				"• Move: drag a hand-placed building's corner to reshape it; Delete removes the selected "
				"corner (or the whole building, if fewer than 3 corners would remain).\n"
				"• Select: click any generated building (hand-placed or OSM-imported) to open its Building "
				"Customization window -- committed changes regenerate that building pool; Delete removes the "
				"selected building entirely. That window includes Levels and Roof Type overrides alongside "
				"tag overrides and forced style, for adjusting an individual building's properties.\n\n"
				"Note: \"Generate Buildings From OSM Asset\" with Replace Existing Building Pools checked also "
				"deletes the hand-placed pool -- generate OSM buildings first, or uncheck that option."))
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
