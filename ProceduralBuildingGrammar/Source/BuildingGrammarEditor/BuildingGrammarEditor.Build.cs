using UnrealBuildTool;

// Editor tooling: a "Tools > Procedural Building Grammar" menu entry (BuildingGrammarEditorModule)
// that file-picks an .osm file and calls straight into BuildingGrammarRuntime's
// UBuildingGenerationLibrary -- a real, clickable v1 rather than an Editor Utility Widget Blueprint
// asset, which can't be authored as a text source file. Thin by design -- almost all logic lives
// in the lower Runtime modules so it stays reusable from a runtime code path too.
public class BuildingGrammarEditor : ModuleRules
{
	public BuildingGrammarEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"EditorFramework",
			"BuildingGrammarCore",
			"BuildingGrammarGeometry",
			"BuildingGrammarRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Slate",
			"SlateCore",
			"EditorScriptingUtilities",
			"Blutility",
			"UMG",
			"UMGEditor",
			"PropertyEditor",
			"ToolMenus",
			"DesktopPlatform",
			// "Generate Buildings from OSM (PCG)..." drives the BuildingGrammarPCG module's
			// alternative pipeline: loads its graph asset, sets its OsmFilePath graph parameter, and
			// triggers a UPCGComponent's Generate() -- see OnGeneratePCGClicked.
			"PCG"
		});
	}
}
