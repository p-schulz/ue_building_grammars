using UnrealBuildTool;

// Pure data + math layer: OSM ingestion, grammar config structs, and the grammar engine itself.
// Deliberately depends only on engine modules available at runtime with no Editor/Slate/Actor
// dependencies, so FBuildingGrammarEngine can be called identically from an editor tool or a
// packaged game's runtime streaming subsystem.
public class BuildingGrammarCore : ModuleRules
{
	public BuildingGrammarCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Json",
			"JsonUtilities",
			"XmlParser"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
