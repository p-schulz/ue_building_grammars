using UnrealBuildTool;

// Actors, the per-cell HISM instance pool, and the world subsystem that drives generation either
// from an editor button press or from runtime proximity streaming.
public class BuildingGrammarRuntime : ModuleRules
{
	public BuildingGrammarRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"BuildingGrammarCore",
			"BuildingGrammarGeometry",
			"GeometryCore",
			"GeometryFramework"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
