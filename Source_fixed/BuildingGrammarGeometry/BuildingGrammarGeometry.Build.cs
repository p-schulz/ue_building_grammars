using UnrealBuildTool;

// Turns BuildingGrammarCore's FMeshSpec output into real Unreal geometry: FDynamicMesh3 hero
// surfaces (walls/roofs), and editor-only baking of those + finite per-style "kit" parts into
// Nanite UStaticMesh assets. The DynamicMesh runtime path (no baking) has no Editor dependency;
// only the StaticMesh-baking functions are WITH_EDITOR-guarded.
public class BuildingGrammarGeometry : ModuleRules
{
	public BuildingGrammarGeometry(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GeometryCore",
			"GeometryFramework",
			"BuildingGrammarCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"MeshDescription",
			"StaticMeshDescription"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[]
			{
				"UnrealEd",
				"GeometryScriptingCore"
			});
		}
	}
}
